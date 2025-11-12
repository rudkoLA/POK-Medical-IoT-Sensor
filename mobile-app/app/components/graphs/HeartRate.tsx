import { useEffect, useState } from "react";
import Graph from "./GraphWidget";
import Fontisto from "@expo/vector-icons/Fontisto";
import { initSocket } from "../../utils/socket";

const HeartRate = () => {
  const [data, setData] = useState<number[]>([]);
  const [loading, setLoading] = useState<boolean>(true);

  useEffect(() => {
    const socket = initSocket();

    const handler = (iotData: { bpm: number }) => {
      const newValue = iotData.bpm;

      if (!isNaN(newValue) && newValue !== null) {
        setData((prev) => {
          const updated = [...prev, newValue];
          return updated.length > 20 ? updated.slice(-20) : updated;
        });
      }

      setLoading(newValue == null || isNaN(newValue));
    };

    socket.on("bpm-data", handler);

    return () => {
      socket.off("bpm-data", handler);
    };
  }, []);

  return (
    <Graph
      name="HeartRate"
      title="Heart Rate"
      data={data}
      baseMin={60}
      baseMax={100}
      accentColor="red"
      maxPoints={20}
      icon={<Fontisto name="heart" size={24} color="white" />}
      labelCount={10}
      dataUnit="BPM"
      loading={loading}
    />
  );
};

export default HeartRate;
