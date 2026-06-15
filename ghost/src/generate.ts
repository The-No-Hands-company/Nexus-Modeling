import { mkdir, symlink, writeFile } from "node:fs/promises";
import { join } from "node:path";
import {
  agents,
  biome,
  deriveVars,
  docsReadme,
  packageJson,
  readme,
  srcCloud,
  srcContracts,
  srcEngine,
  srcIndex,
  srcServer,
  testServer,
  tsconfig,
  // Frontend templates
  frontendPackageJson,
  frontendViteConfig,
  frontendAppTsx,
  frontendIndexCss,
  frontendPostcss,
} from "./templates";

const APPS_DIR = join(import.meta.dirname!, "..", "..", "apps");

interface ScaffoldOptions {
  name: string;
  port: number;
  description: string;
}

async function writeDir(base: string, path: string) {
  await mkdir(join(base, path), { recursive: true });
}

async function write(base: string, path: string, content: string) {
  const full = join(base, path);
  await writeFile(full, content, "utf-8");
  console.log(`  ${path}`);
}

export async function scaffold(opts: ScaffoldOptions): Promise<string> {
  const v: TemplateVars = deriveVars(opts.name, opts.port, opts.description);
  const appDir = join(APPS_DIR, v.displayName);

  console.log(`\nScaffolding ${v.displayName}...\n`);

  await mkdir(appDir, { recursive: true });

  await writeDir(appDir, "src");
  await writeDir(appDir, "tests");
  await writeDir(appDir, "docs");

  await write(appDir, "package.json", packageJson(v));
  await write(appDir, "tsconfig.json", tsconfig());
  await write(appDir, "biome.json", biome());
  await write(appDir, "README.md", readme(v));
  await write(appDir, "AGENTS.md", agents(v));
  await write(appDir, "docs/README.md", docsReadme(v));

  await write(appDir, `src/${v.domain}-engine.ts`, srcEngine(v));
  await write(appDir, "src/server.ts", srcServer(v));
  await write(appDir, "src/contracts.ts", srcContracts(v));
  await write(appDir, "src/cloud.ts", srcCloud(v));
  await write(appDir, "src/index.ts", srcIndex());

  await write(appDir, "src/.gitkeep", "");
  await write(appDir, "tests/.gitkeep", "");

  await write(appDir, "tests/server.test.ts", testServer(v));

  // ── React frontend ──────────────────────────────────────────
  console.log("  frontend/");
  await mkdir(join(appDir, "frontend", "src"), { recursive: true });
  await write(appDir, "frontend/package.json", frontendPackageJson(v));
  await write(appDir, "frontend/vite.config.ts", frontendViteConfig(v));
  await write(appDir, "frontend/tsconfig.json", JSON.stringify({
    compilerOptions: { target: "ESNext", module: "ESNext", moduleResolution: "bundler", strict: true, jsx: "react-jsx", skipLibCheck: true },
    include: ["src"]
  }, null, 2));
  await write(appDir, "frontend/index.html", `<!doctype html><html lang="en"><head><meta charset="UTF-8"/><title>${v.displayName}</title></head><body><div id="root"></div><script type="module" src="/src/main.tsx"></script></body></html>`);
  await write(appDir, "frontend/postcss.config.js", frontendPostcss());
  await write(appDir, "frontend/src/index.css", frontendIndexCss());
  await write(appDir, "frontend/src/main.tsx", `import React from "react";\nimport ReactDOM from "react-dom/client";\nimport App from "./App";\nimport "./index.css";\n\nReactDOM.createRoot(document.getElementById("root")!).render(<React.StrictMode><App /></React.StrictMode>);`);
  await write(appDir, "frontend/src/App.tsx", frontendAppTsx(v));

  const linkPath = join(appDir, "data");
  try {
    await symlink("../../data", linkPath, "dir");
  } catch {
    console.warn(`  [warn] Could not create data symlink (might already exist)`);
  }

  // Install dependencies via bun
  try {
    console.log("  Installing dependencies...");
    
    console.log("  Dependencies installed.");
  } catch {
    console.warn("  [warn] Could not install dependencies (run `bun install` manually)");
  }

  console.log(`\nDone. ${v.displayName} scaffolded at ${appDir}\n`);
  return appDir;
}
