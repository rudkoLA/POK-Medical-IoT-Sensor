import { createBottomTabNavigator } from "@react-navigation/bottom-tabs";
import Health from "../pages/Health";
import Live from "../pages/Live";
import Device from "../pages/Device";
import Profile from "../pages/Profile";
import Entypo from "@expo/vector-icons/Entypo";
import Ionicons from "@expo/vector-icons/Ionicons";
import AntDesign from "@expo/vector-icons/AntDesign";

const Tab = createBottomTabNavigator();

const TabNavigator = () => {
  return (
    <Tab.Navigator
      screenOptions={({ route }) => ({
        headerShown: false,
        tabBarStyle: { backgroundColor: "#1f1420ff", borderTopWidth: 0 },
        tabBarActiveTintColor: "#f017ecff",
        tabBarInactiveTintColor: "#64496dff",
        tabBarIcon: ({ color, size }) => {
          const icons: Record<string, React.ReactElement> = {
            Health: <Entypo name="heart" size={size} color={color} />,
            Live: <AntDesign name="line-chart" size={size} color={color} />,
            Device: <Ionicons name="watch" size={size} color={color} />,
            Profile: <Ionicons name="person" size={size} color={color} />,
          };
          return icons[route.name];
        },
      })}
    >
      <Tab.Screen name="Health" component={Health} />
      <Tab.Screen name="Live" component={Live} />
      <Tab.Screen name="Device" component={Device} />
      <Tab.Screen name="Profile" component={Profile} />
    </Tab.Navigator>
  );
}

export default TabNavigator;