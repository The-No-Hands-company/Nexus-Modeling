import { createMonitorServer } from "./server";

const { close } = createMonitorServer();

process.on("SIGTERM", () => { close(); process.exit(0); });
process.on("SIGINT", () => { close(); process.exit(0); });
