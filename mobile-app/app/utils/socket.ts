import { io, Socket } from "socket.io-client";

let socket: Socket | null = null;

const SERVER_ADDRESS = "https://iot-mobile-backend-hxhrf2fsd6cgh0ft.germanywestcentral-01.azurewebsites.net";

export const initSocket = (): Socket => {
  if (!socket) {
    socket = io(SERVER_ADDRESS);
  }
  return socket;
};

export const getSocket = (): Socket | null => socket;