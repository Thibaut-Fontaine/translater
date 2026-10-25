package com.xtrans.app

import android.Manifest
import android.content.pm.PackageManager
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.speech.tts.TextToSpeech
import com.facebook.react.bridge.Promise
import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.module.annotations.ReactModule
import java.io.File
import java.util.Locale
import java.util.concurrent.Executors

/**
 * TurboModule around the shared C++ core.
 *
 * Audio is captured here, on a background thread, and handed straight to
 * `nativeFinishVoice`. It never becomes a JS array. Long operations (model
 * load, transcription + translation) run on one worker so the JS thread only
 * ever polls cheap cached getters.
 */
@ReactModule(name = XtransModule.NAME)
class XtransModule(private val reactContext: ReactApplicationContext) :
  NativeXtransSessionSpec(reactContext) {

  companion object {
    const val NAME = "XtransSession"
    private const val SAMPLE_RATE = 16000

    init {
      System.loadLibrary("xtrans")
    }
  }

  private val worker = Executors.newSingleThreadExecutor()
  private val lock = Any()

  @Volatile private var handle: Long = 0
  @Volatile private var capturing = false

  private var captureThread: Thread? = null
  private var recorder: AudioRecord? = null
  private var samples = ShortArray(SAMPLE_RATE * 5)
  private var sampleCount = 0

  private var tts: TextToSpeech? = null
  @Volatile private var ttsReady = false

  init {
    tts = TextToSpeech(reactContext) { status ->
      ttsReady = status == TextToSpeech.SUCCESS
    }
  }

  private fun modelsDirectory(): String {
    val base = reactContext.getExternalFilesDir(null) ?: reactContext.filesDir
    return File(base, "models").absolutePath
  }

  // --- model lifecycle -----------------------------------------------------

  override fun openModels(modelsDir: String, promise: Promise) {
    worker.execute {
      closeHandle()
      val opened = nativeOpen(modelsDir)
      handle = opened
      promise.resolve(opened != 0L)
    }
  }

  override fun isReady(): Boolean = handle != 0L && nativeReady(handle)

  override fun modelsDir(): String = modelsDirectory()

  override fun modelsStatus(): String = nativeModelsStatus(modelsDirectory())

  override fun configure(from: Double, to: Double, mode: Double): Boolean {
    val h = handle
    if (h == 0L) return false
    return nativeConfigure(h, from.toInt(), to.toInt(), mode.toInt())
  }

  // --- capture -------------------------------------------------------------

  override fun startVoice(): Boolean {
    val h = handle
    if (h == 0L || capturing) return false
    if (reactContext.checkSelfPermission(Manifest.permission.RECORD_AUDIO) !=
      PackageManager.PERMISSION_GRANTED
    ) {
      return false
    }
    if (!nativeBeginVoice(h)) return false

    val min = AudioRecord.getMinBufferSize(
      SAMPLE_RATE,
      AudioFormat.CHANNEL_IN_MONO,
      AudioFormat.ENCODING_PCM_16BIT,
    )
    val bufferSize = if (min > 0) maxOf(min, SAMPLE_RATE / 2) else SAMPLE_RATE / 2
    val input = try {
      AudioRecord(
        MediaRecorder.AudioSource.VOICE_RECOGNITION,
        SAMPLE_RATE,
        AudioFormat.CHANNEL_IN_MONO,
        AudioFormat.ENCODING_PCM_16BIT,
        bufferSize,
      )
    } catch (e: SecurityException) {
      nativeCancel(h)
      return false
    }

    if (input.state != AudioRecord.STATE_INITIALIZED) {
      input.release()
      nativeCancel(h)
      return false
    }

    synchronized(lock) {
      samples = ShortArray(SAMPLE_RATE * 5)
      sampleCount = 0
      recorder = input
      capturing = true
    }

    input.startRecording()
    captureThread = Thread({
      val buffer = ShortArray(1600)
      while (capturing) {
        val read = input.read(buffer, 0, buffer.size)
        if (read > 0) append(buffer, read)
      }
    }, "xtrans-capture").also { it.start() }

    return true
  }

  private fun append(buffer: ShortArray, read: Int) {
    synchronized(lock) {
      if (sampleCount + read > samples.size) {
        samples = samples.copyOf(maxOf(samples.size * 2, sampleCount + read))
      }
      System.arraycopy(buffer, 0, samples, sampleCount, read)
      sampleCount += read
    }
  }

  private fun stopCapture(): ShortArray {
    capturing = false
    captureThread?.let { thread ->
      try {
        thread.join(1000)
      } catch (e: InterruptedException) {
        Thread.currentThread().interrupt()
      }
    }
    captureThread = null
    synchronized(lock) {
      recorder?.let { input ->
        try {
          if (input.recordingState == AudioRecord.RECORDSTATE_RECORDING) input.stop()
        } catch (e: IllegalStateException) {
          // Already stopped by the system.
        }
        input.release()
      }
      recorder = null
      val pcm = samples.copyOf(sampleCount)
      sampleCount = 0
      return pcm
    }
  }

  override fun stopVoice(promise: Promise) {
    val h = handle
    if (h == 0L) {
      promise.resolve(1.0)
      return
    }
    worker.execute {
      val pcm = stopCapture()
      val status = nativeFinishVoice(h, pcm)
      promise.resolve(status.toDouble())
    }
  }

  // --- text ----------------------------------------------------------------

  override fun translate(text: String, promise: Promise) {
    val h = handle
    if (h == 0L) {
      promise.resolve(1.0)
      return
    }
    // NFKC here, in the platform's Unicode implementation: the C++ BPE
    // encoder expects already-normalised text.
    val normalized = java.text.Normalizer.normalize(text, java.text.Normalizer.Form.NFKC)
    worker.execute {
      promise.resolve(nativeTranslate(h, normalized).toDouble())
    }
  }

  override fun cancel() {
    val h = handle
    if (h == 0L) return
    worker.execute {
      stopCapture()
      nativeCancel(h)
    }
  }

  // --- polling getters -----------------------------------------------------

  override fun state(): Double {
    val h = handle
    return if (h == 0L) 0.0 else nativeState(h).toDouble()
  }

  override fun transcript(): String {
    val h = handle
    return if (h == 0L) "" else nativeTranscript(h)
  }

  override fun translation(): String {
    val h = handle
    return if (h == 0L) "" else nativeTranslation(h)
  }

  override fun lastError(): String {
    val h = handle
    return if (h == 0L) normalizedOpenError() else nativeLastError(h)
  }

  private fun normalizedOpenError(): String = nativeOpenError()

  // --- OS voice ------------------------------------------------------------

  override fun speak(text: String, languageTag: String) {
    val engine = tts ?: return
    if (!ttsReady || text.isEmpty()) return
    val locale = Locale.forLanguageTag(languageTag)
    if (engine.isLanguageAvailable(locale) < TextToSpeech.LANG_AVAILABLE) return
    engine.language = locale
    engine.speak(text, TextToSpeech.QUEUE_FLUSH, null, "xtrans")
  }

  override fun stopSpeaking() {
    tts?.stop()
  }

  // --- teardown ------------------------------------------------------------

  private fun closeHandle() {
    val h = handle
    handle = 0
    if (h != 0L) nativeClose(h)
  }

  override fun invalidate() {
    stopCapture()
    worker.execute { closeHandle() }
    worker.shutdown()
    tts?.stop()
    tts?.shutdown()
    tts = null
    super.invalidate()
  }

  // --- JNI -----------------------------------------------------------------

  private external fun nativeOpen(modelsDir: String): Long
  private external fun nativeOpenError(): String
  private external fun nativeReady(handle: Long): Boolean
  private external fun nativeClose(handle: Long)
  private external fun nativeConfigure(handle: Long, from: Int, to: Int, mode: Int): Boolean
  private external fun nativeBeginVoice(handle: Long): Boolean
  private external fun nativeFinishVoice(handle: Long, samples: ShortArray): Int
  private external fun nativeTranslate(handle: Long, text: String): Int
  private external fun nativeCancel(handle: Long)
  private external fun nativeState(handle: Long): Int
  private external fun nativeTranscript(handle: Long): String
  private external fun nativeTranslation(handle: Long): String
  private external fun nativeLastError(handle: Long): String
  private external fun nativeModelsStatus(modelsDir: String): String
}
