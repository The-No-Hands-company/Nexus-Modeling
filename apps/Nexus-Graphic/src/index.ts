import { createServer } from "./server";

const serverPromise = createServer();
if (serverPromise instanceof Promise) {
  serverPromise.then(({ close }) => {
    process.on("SIGTERM", () => { close(); process.exit(0); });
    process.on("SIGINT", () => { close(); process.exit(0); });
  });
} else {
  const { close } = serverPromise as any;
  process.on("SIGTERM", () => { close(); process.exit(0); });
  process.on("SIGINT", () => { close(); process.exit(0); });
}
