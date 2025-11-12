import { useEffect, useState } from "react";
import Graph from "./GraphWidget";
import Fontisto from "@expo/vector-icons/Fontisto";
import { initSocket } from "../../utils/socket";

const SPO2 = () => {
  const [data, setData] = useState<number[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const socket = initSocket();

    const handler = (iotData: { spo2: number }) => {
      const newValue = iotData.spo2;

      if (!isNaN(newValue) && newValue !== null) {
        setData((prev) => {
          const updated = [...prev, newValue];
          return updated.length > 20 ? updated.slice(-20) : updated;
        });
      }

      setLoading(newValue == null || isNaN(newValue));
    };

    socket.on("spo2-data", handler);

    return () => {
      socket.off("spo2-data", handler);
    };
  }, []);

  return (
    <Graph
      name="SpO2"
      title="Blood Oxygen Level"
      data={data}
      baseMin={80}
      baseMax={100}
      accentColor="green"
      maxPoints={20}
      icon={<Fontisto name="blood-drop" size={24} color="white" />}
      labelCount={5}
      dataUnit="%"
      loading={loading}
    />
  );
};

export default SPO2;
