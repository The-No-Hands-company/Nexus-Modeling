import { createFilesServer } from "./server";

const { server, close } = createFilesServer();

process.on("SIGTERM", () => { close(); process.exit(0); });
process.on("SIGINT", () => { close(); process.exit(0); });
