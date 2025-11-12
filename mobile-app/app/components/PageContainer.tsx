import { ReactNode } from "react";
import { View } from "react-native";

type PageContainerProps = {
  children?: ReactNode;
};
const PageContainer = ({ children }: PageContainerProps) => {
  return (
    <View
      style={{
        width: "100%",
        height: "100%",
        paddingTop: 40,
        paddingHorizontal: 12,
        backgroundColor: "#29212eff",
        gap: 16,
      }}
    >
      {children}
    </View>
  );
};

export default PageContainer;
