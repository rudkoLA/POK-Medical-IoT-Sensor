import { Text, View } from "react-native";

type PageHeaderProps = {
  name?: string;
};

const PageHeader = ({ name }: PageHeaderProps) => {
  return (
    <View
      style={{
        width: "100%",
        display: "flex",
        justifyContent: "space-between",
        alignItems: "flex-start",
        flexDirection: "row",
      }}
    >
      <Text style={{ fontSize: 32, color: "white" }}>{name}</Text>
    </View>
  );
};

export default PageHeader;
