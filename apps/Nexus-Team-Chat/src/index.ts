import { createTeamChatServer } from "./server";
const { close } = createTeamChatServer();
process.on("SIGTERM", () => { close(); process.exit(0); });
process.on("SIGINT", () => { close(); process.exit(0); });
