import { createApiServer } from "./server";
const { close } = createApiServer();
process.on("SIGTERM", () => { close(); process.exit(0); });
process.on("SIGINT", () => { close(); process.exit(0); });
