import { GestureHandlerRootView } from "react-native-gesture-handler";
import { Platform } from "react-native";
import { NavigationContainer } from "@react-navigation/native";
import MainStack from "./navigation/MainStack";

const App = () => {
  const content = <MainStack />;

  return Platform.OS === "web" ? (
    <NavigationContainer>{content}</NavigationContainer>
  ) : (
    <GestureHandlerRootView style={{ flex: 1 }}>
      {content}
    </GestureHandlerRootView>
  );
}

export default App;