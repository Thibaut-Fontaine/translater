// The real package needs a native provider; in Jest, insets are zero.
const React = require('react');
const {View} = require('react-native');

const insets = {top: 0, right: 0, bottom: 0, left: 0};

module.exports = {
  SafeAreaProvider: ({children}) => children,
  SafeAreaView: ({children, edges, ...props}) => React.createElement(View, props, children),
  SafeAreaInsetsContext: React.createContext(insets),
  SafeAreaFrameContext: React.createContext({x: 0, y: 0, width: 0, height: 0}),
  useSafeAreaInsets: () => insets,
  useSafeAreaFrame: () => ({x: 0, y: 0, width: 0, height: 0}),
  initialWindowMetrics: {insets, frame: {x: 0, y: 0, width: 0, height: 0}},
};
