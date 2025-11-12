import { createNativeStackNavigator } from "@react-navigation/native-stack";
import TabNavigator from "./TabNavigator";
// import HeartRate from "../pages/HeartRate";
// import BloodOxygen from "../pages/BloodOxygen";
// import SubPage3 from "../pages/SubPage3";
// import SubPage4 from "../pages/SubPage4";

export type Pages = {
  Tabs: undefined;
  Health: undefined;
  Reports: undefined;
  Device: undefined;
  Profile: undefined;
  HeartRate: undefined;
  BloodOxygen: undefined;
  SubPage3: undefined;
  SubPage4: undefined;
};

const Stack = createNativeStackNavigator<Pages>();

const MainStack = () => {
  return (
    <Stack.Navigator screenOptions={{ headerShown: false }}>
      <Stack.Screen name="Tabs" component={TabNavigator} />
      {/* <Stack.Screen name="HeartRate" component={HeartRate} />
      <Stack.Screen name="BloodOxygen" component={BloodOxygen} />
      <Stack.Screen name="SubPage3" component={SubPage3} />
      <Stack.Screen name="SubPage4" component={SubPage4} /> */}
    </Stack.Navigator>
  );
}

export default MainStack;