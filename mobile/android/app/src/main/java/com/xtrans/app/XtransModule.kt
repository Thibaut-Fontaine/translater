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
import java.io.FileOutputStream
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL
import java.security.MessageDigest
import java.util.Locale
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.Executors
import org.json.JSONArray
import org.json.JSONObject

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

  // Downloads: one at a time, resumable, verified against the published digest.
  private data class ModelSpec(
    val name: String,
    val url: String,
    val sha256: String,
    val bytes: Long,
  )

  private class DownloadState {
    @Volatile var state: String = "missing"
    @Volatile var received: Long = 0
    @Volatile var total: Long = 0
    @Volatile var error: String? = null
  }

  private val modelSpecs = listOf(
    ModelSpec(
      "nllb_bpe.bin",
      "https://thibautfontaine.space/translater/models/nllb_bpe.bin",
      "441e579bff7c371e140064eef33782f4f785020890de736d77c6af08cc73cf05",
      12_587_091,
    ),
    ModelSpec(
      "ggml-base-q5_1.bin",
      "https://huggingface.co/ggerganov/whisper.cpp/resolve/5359861c739e955e79d9a303bcbc70fb988958b1/ggml-base-q5_1.bin",
      "422f1ae452ade6f30a004d7e5c6a43195e4433bc370bf23fac9cc591f01a8898",
      59_707_625,
    ),
    ModelSpec(
      "encoder_model_int8.onnx",
      "https://huggingface.co/Xenova/nllb-200-distilled-600M/resolve/261c31d1a5732c67cdd16d80e8d6088507c7ccea/onnx/encoder_model_int8.onnx",
      "da9234723304e9f76216b67a43ede4fe65393cbf5942a7e4a84595782a4bc161",
      415_346_576,
    ),
    ModelSpec(
      "decoder_model_quantized.onnx",
      "https://huggingface.co/Xenova/nllb-200-distilled-600M/resolve/261c31d1a5732c67cdd16d80e8d6088507c7ccea/onnx/decoder_model_quantized.onnx",
      "ddea619b640379609719becf91a488c5e6ce4c4b2052efbb5388edaed465a552",
      470_533_055,
    ),
    ModelSpec(
      "decoder_with_past_model_quantized.onnx",
      "https://huggingface.co/Xenova/nllb-200-distilled-600M/resolve/261c31d1a5732c67cdd16d80e8d6088507c7ccea/onnx/decoder_with_past_model_quantized.onnx",
      "374293cb4bb59cd29810851a7cfbc568accb8efdc1a953ac04722e30adaf2d0b",
      445_006_987,
    ),
  )

  private val downloadStates = ConcurrentHashMap<String, DownloadState>()
  @Volatile private var downloading = false
  @Volatile private var cancelRequested = false
  @Volatile private var activeConnection: HttpURLConnection? = null
  private var downloadThread: Thread? = null

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

  private fun stateOf(name: String): DownloadState =
    downloadStates.getOrPut(name) { DownloadState() }

  override fun modelsStatus(): String {
    val dir = modelsDirectory()
    val array = JSONArray()
    for (spec in modelSpecs) {
      val file = File(dir, spec.name)
      val part = File(dir, spec.name + ".part")
      val present = file.exists() && file.length() == spec.bytes
      val state = stateOf(spec.name)
      val entry = JSONObject()
      entry.put("name", spec.name)
      entry.put("present", present)
      entry.put("bytes", spec.bytes)
      entry.put(
        "state",
        when {
          present -> "ready"
          state.state != "missing" -> state.state
          else -> "missing"
        },
      )
      entry.put("received", if (present) spec.bytes else maxOf(state.received, part.length()))
      entry.put("total", if (state.total > 0) state.total else spec.bytes)
      if (state.error != null) entry.put("error", state.error) else entry.put("error", JSONObject.NULL)
      array.put(entry)
    }
    return array.toString()
  }

  override fun downloadModels(): Boolean {
    if (downloading) return false
    val dir = modelsDirectory()
    File(dir).mkdirs()
    val pending = modelSpecs.filter { !File(dir, it.name).exists() }
    if (pending.isEmpty()) return false

    cancelRequested = false
    downloading = true
    for (spec in pending) {
      val state = stateOf(spec.name)
      state.state = "queued"
      state.error = null
      state.total = spec.bytes
      state.received = File(dir, spec.name + ".part").length()
    }
    downloadThread = Thread({ runDownloads(dir, pending) }, "xtrans-download").also { it.start() }
    return true
  }

  override fun cancelDownloads() {
    cancelRequested = true
    activeConnection?.let {
      try {
        it.disconnect()
      } catch (_: Throwable) {
        // Already closed.
      }
    }
    for (spec in modelSpecs) {
      val state = downloadStates[spec.name] ?: continue
      if (state.state == "downloading" || state.state == "queued" || state.state == "verifying") {
        state.state = "cancelled"
      }
    }
  }

  private fun runDownloads(dir: String, pending: List<ModelSpec>) {
    try {
      for (spec in pending) {
        if (cancelRequested) return
        try {
          downloadOne(dir, spec)
        } catch (e: Throwable) {
          if (cancelRequested) return
          val state = stateOf(spec.name)
          state.state = "failed"
          state.error = e.message ?: e.javaClass.simpleName
          return
        }
      }
    } finally {
      downloading = false
      activeConnection = null
    }
  }

  private fun downloadOne(dir: String, spec: ModelSpec) {
    val target = File(dir, spec.name)
    val part = File(dir, spec.name + ".part")
    val state = stateOf(spec.name)
    state.state = "downloading"
    state.total = spec.bytes
    state.error = null

    var offset = if (part.exists()) part.length() else 0
    state.received = offset

    var connection = open(spec, offset)
    var code = connection.responseCode
    if (code == 416) {
      // The partial file no longer matches the remote object.
      part.delete()
      offset = 0
      connection.disconnect()
      connection = open(spec, offset)
      code = connection.responseCode
    }
    if (code != HttpURLConnection.HTTP_OK && code != HttpURLConnection.HTTP_PARTIAL) {
      connection.disconnect()
      throw IOException("HTTP $code")
    }

    val start = if (code == HttpURLConnection.HTTP_PARTIAL && offset > 0) offset else 0
    val remaining = connection.contentLengthLong
    state.total = if (remaining > 0) start + remaining else spec.bytes
    state.received = start

    try {
      FileOutputStream(part, start > 0).use { output ->
        connection.inputStream.use { input ->
          val buffer = ByteArray(128 * 1024)
          var done = start
          while (!cancelRequested) {
            val read = input.read(buffer)
            if (read < 0) break
            output.write(buffer, 0, read)
            done += read
            state.received = done
          }
        }
      }
    } finally {
      connection.disconnect()
      activeConnection = null
    }
    if (cancelRequested) throw IOException("cancelled")

    state.state = "verifying"
    if (!sha256(part).equals(spec.sha256, ignoreCase = true)) {
      part.delete()
      throw IOException("checksum mismatch")
    }
    if (!part.renameTo(target)) {
      part.copyTo(target, overwrite = true)
      part.delete()
    }
    state.state = "ready"
    state.received = target.length()
    state.total = spec.bytes
  }

  private fun open(spec: ModelSpec, offset: Long): HttpURLConnection {
    val connection = URL(spec.url).openConnection() as HttpURLConnection
    connection.instanceFollowRedirects = true
    connection.connectTimeout = 20_000
    connection.readTimeout = 60_000
    connection.setRequestProperty("User-Agent", "xtrans-android")
    if (offset > 0) connection.setRequestProperty("Range", "bytes=$offset-")
    activeConnection = connection
    return connection
  }

  private fun sha256(file: File): String {
    val digest = MessageDigest.getInstance("SHA-256")
    file.inputStream().use { input ->
      val buffer = ByteArray(1024 * 1024)
      while (true) {
        val read = input.read(buffer)
        if (read < 0) break
        digest.update(buffer, 0, read)
      }
    }
    return digest.digest().joinToString("") { "%02x".format(it) }
  }

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
    cancelDownloads()
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
