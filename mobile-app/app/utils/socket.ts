import { io, Socket } from "socket.io-client";

let socket: Socket | null = null;

const SERVER_IP = "";

export const initSocket = (): Socket => {
  if (SERVER_IP.length === 0) {
    console.error("Replace SERVER_IP with network ip address in mobile-app/app/utils/socket.ts")
  }
  if (!socket) {
    // ~~~>> TODO: replace with server ip once hosted on azure
    socket = io(`http://${SERVER_IP}:3000`);
  }
  return socket;
};

export const getSocket = (): Socket | null => socket;