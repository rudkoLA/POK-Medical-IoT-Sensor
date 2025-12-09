import express from "express";
import { Server } from "socket.io";
import http from "http";
import { EventHubConsumerClient } from "@azure/event-hubs";

import "dotenv/config";

const EVENT_HUB_CONNECTION_STRING = process.env.EVENT_HUB_CONNECTION_STRING;
const EVENT_HUB_NAME = "iothub-ehub-medicaliot-55873260-17a8a13b5f";
const CONSUMER_GROUP = "$Default";

const PORT = process.env.PORT;

const app = express();
const server = http.createServer(app);
const io = new Server(server, { cors: { origin: "*" } });

const consumer = new EventHubConsumerClient(
  CONSUMER_GROUP,
  EVENT_HUB_CONNECTION_STRING,
  EVENT_HUB_NAME
);

consumer.subscribe({
  processEvents: async (events) => {
    for (const e of events) {
      console.log(e.body);

      if (e.body?.bpm) {
        io.emit("bpm-data", { bpm: e.body.bpm });
      }
      if (e.body?.spo2) {
        io.emit("spo2-data", { spo2: e.body.spo2 });
      }
    }
  },
  processError: async (err) => console.error(err),
});

server.listen(PORT, '0.0.0.0', () =>
  console.log(`Server running on port ${PORT}`)
);
