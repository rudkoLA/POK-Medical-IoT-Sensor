import express from "express";
import { Server } from "socket.io";
import http from "http";
import { EventHubConsumerClient } from "@azure/event-hubs";
import { Client } from "azure-iothub";
import {
  eventHubConnectionString,
  eventHubName,
  consumerGroup,
  serviceConnectionString,
} from "./env.js";

const app = express();
const server = http.createServer(app);
const io = new Server(server, { cors: { origin: "*" } });

const consumer = new EventHubConsumerClient(
  consumerGroup,
  eventHubConnectionString,
  eventHubName
);

let bpmBatch = [];
let spo2Batch = [];

consumer.subscribe({
  processEvents: async (events) => {
    for (const e of events) {
      console.log(e.body);
      bpmBatch.push(e.body?.bpm);
      spo2Batch.push(e.body?.spo2);
    }
  },
  processError: async (err) => console.error(err),
});

// setInterval(() => {
//   bpmBatch.push(80 + Math.random() * 40);
// }, 20 + Math.random() * 100);


const getAvg = (arr) => {
  const filteredArr = arr.filter((v) => typeof v === "number" && !isNaN(v));

  return filteredArr.length > 0
    ? filteredArr.reduce((a, b) => a + b, 0) / filteredArr.length
    : NaN;
};

setInterval(() => {
  const avgBPM = getAvg(bpmBatch);
  const avgSpo2 = getAvg(spo2Batch);

  io.emit("bpm-data", { bpm: avgBPM });
  io.emit("spo2-data", { spo2: avgSpo2 });

  bpmBatch = [];
  spo2Batch = [];
}, 1000);

server.listen(3000, () => console.log("Server running on port 3000"));
