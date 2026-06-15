import { useState, useEffect, useCallback } from "react";

// ── Types ──

interface Page {
  slug: string; title: string; content: string;
  category?: string | null; tags?: string[];
  version: number; created_at: string; updated_at: string;
}
interface PageSummary { slug: string; title: string; updated_at: string; }
interface Revision { slug: string; version: number; title: string; content: string; editor_id: string; change_summary: string; created_at: string; }
interface Category { name: string; count: number; }

// ── API ──

const API = "/api/v1";
const fetchJson = (url: string, opts?: RequestInit) => fetch(url, opts).then(r => r.ok ? r.json() : Promise.reject(r.status));

// ── Simple markdown renderer ──

function renderMd(text: string): string {
  return text
    .replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
    .replace(/^### (.+)$/gm, "<h3 class='text-xl font-bold mt-4 mb-2'>$1</h3>")
    .replace(/^## (.+)$/gm, "<h2 class='text-2xl font-bold mt-6 mb-2'>$1</h2>")
    .replace(/^# (.+)$/gm, "<h1 class='text-3xl font-bold mt-6 mb-3'>$1</h1>")
    .replace(/\*\*(.+?)\*\*/g, "<strong>$1</strong>")
    .replace(/\*(.+?)\*/g, "<em>$1</em>")
    .replace(/`(.+?)`/g, "<code class='bg-zinc-800 px-1 rounded'>$1</code>")
    .replace(/\[(.+?)\]\((.+?)\)/g, "<a href='$2' class='text-blue-400 hover:underline'>$1</a>")
    .replace(/^- (.+)$/gm, "<li class='ml-4'>$1</li>")
    .replace(/\n\n/g, "</p><p class='mb-3'>")
    .replace(/^(.+)$/gm, "<p class='mb-3'>$1</p>");
}

// ── App ──

export default function App() {
  const [view, setView] = useState<"browse" | "read" | "edit" | "history">("browse");
  const [slug, setSlug] = useState("");
  const [pages, setPages] = useState<PageSummary[]>([]);
  const [page, setPage] = useState<Page | null>(null);
  const [revisions, setRevisions] = useState<Revision[]>([]);
  const [categories, setCategories] = useState<Category[]>([]);
  const [search, setSearch] = useState("");
  const [selectedCat, setSelectedCat] = useState("");

  const loadPages = useCallback(async () => {
    try { setPages((await fetchJson(`${API}/pages`)).items); } catch {}
  }, []);
  const loadCategories = useCallback(async () => {
    try { setCategories((await fetchJson(`${API}/categories`)).categories); } catch {}
  }, []);

  useEffect(() => { loadPages(); loadCategories(); }, [loadPages, loadCategories]);

  const openPage = async (s: string) => {
    try { setPage(await fetchJson(`${API}/pages/${s}`)); setSlug(s); setView("read"); } catch {}
  };
  const openEdit = (s: string) => { setSlug(s); setView("edit"); };
  const openHistory = async (s: string) => {
    try { setRevisions((await fetchJson(`${API}/pages/${s}/revisions`)).items); setSlug(s); setView("history"); } catch {}
  };
  const goBrowse = () => { setView("browse"); setSearch(""); setSelectedCat(""); };
  const randomPage = async () => {
    try { const p = await fetchJson(`${API}/pages/random`); openPage(p.slug); } catch {}
  };

  const filteredPages = pages.filter(p => {
    if (search && !p.title.toLowerCase().includes(search.toLowerCase())) return false;
    if (selectedCat) {
      const catPage = pages.find(x => x.slug === p.slug);
      return catPage !== undefined;
    }
    return true;
  });

  return (
    <div className="min-h-screen bg-zinc-950 text-zinc-200">
      <header className="border-b border-zinc-800 bg-zinc-900 sticky top-0 z-10">
        <div className="max-w-5xl mx-auto px-4 py-3 flex items-center gap-4">
          <button onClick={goBrowse} className="text-xl font-bold text-blue-400 hover:text-blue-300">Nexus-Wiki</button>
          <div className="flex-1 flex gap-2">
            <input value={search} onChange={e => setSearch(e.target.value)}
              placeholder="Search articles..." className="flex-1 bg-zinc-800 px-3 py-1.5 rounded text-sm border border-zinc-700 focus:border-blue-500 outline-none" />
            <button onClick={randomPage} className="px-3 py-1.5 text-sm bg-zinc-800 rounded hover:bg-zinc-700">Random</button>
            <button onClick={() => openEdit("")} className="px-3 py-1.5 text-sm bg-blue-600 rounded hover:bg-blue-500">+ New</button>
          </div>
        </div>
      </header>

      <main className="max-w-5xl mx-auto px-4 py-6 flex gap-6">
        {/* Sidebar */}
        {view === "browse" && (
          <aside className="w-48 shrink-0">
            <h3 className="text-xs uppercase text-zinc-500 font-semibold mb-2">Categories</h3>
            <div className="space-y-1">
              <button onClick={() => setSelectedCat("")}
                className={`block w-full text-left text-sm px-2 py-0.5 rounded ${!selectedCat ? "bg-blue-600/20 text-blue-300" : "hover:bg-zinc-800"}`}>All</button>
              {categories.map(c => (
                <button key={c.name} onClick={() => setSelectedCat(c.name)}
                  className={`block w-full text-left text-sm px-2 py-0.5 rounded flex justify-between ${selectedCat === c.name ? "bg-blue-600/20 text-blue-300" : "hover:bg-zinc-800"}`}>
                  <span>{c.name}</span><span className="text-zinc-600">{c.count}</span>
                </button>
              ))}
            </div>
          </aside>
        )}

        {/* Content */}
        <div className="flex-1 min-w-0">
          {view === "browse" && (
            <div>
              <h2 className="text-lg font-semibold mb-4">{search ? `Search: "${search}"` : "All Articles"}</h2>
              <div className="space-y-1">
                {filteredPages.map(p => (
                  <button key={p.slug} onClick={() => openPage(p.slug)}
                    className="block w-full text-left px-3 py-2 rounded hover:bg-zinc-800/50 transition-colors">
                    <span className="text-blue-400 hover:text-blue-300">{p.title}</span>
                    <span className="text-xs text-zinc-600 ml-3">{new Date(p.updated_at).toLocaleDateString()}</span>
                  </button>
                ))}
              </div>
              {filteredPages.length === 0 && <p className="text-zinc-600 text-sm">No articles yet. <button onClick={() => openEdit("")} className="text-blue-400 hover:underline">Create the first one</button>.</p>}
            </div>
          )}

          {view === "read" && page && (
            <div>
              <div className="flex items-center gap-3 mb-6">
                <button onClick={goBrowse} className="text-sm text-zinc-500 hover:text-zinc-300">← Back</button>
                <button onClick={() => openEdit(page.slug)} className="text-sm px-2 py-0.5 bg-zinc-800 rounded hover:bg-zinc-700">Edit</button>
                <button onClick={() => openHistory(page.slug)} className="text-sm px-2 py-0.5 bg-zinc-800 rounded hover:bg-zinc-700">History</button>
              </div>
              <h1 className="text-3xl font-bold mb-4">{page.title}</h1>
              {page.category && <span className="text-xs bg-zinc-800 px-2 py-0.5 rounded mr-2">{page.category}</span>}
              {page.tags?.map(t => <span key={t} className="text-xs text-zinc-500 mr-2">#{t}</span>)}
              <p className="text-xs text-zinc-600 mt-2 mb-6">Version {page.version} · Updated {new Date(page.updated_at).toLocaleString()}</p>
              <div className="prose prose-invert max-w-none leading-relaxed" dangerouslySetInnerHTML={{ __html: renderMd(page.content) }} />
            </div>
          )}

          {view === "edit" && (
            <Editor slug={slug} page={page} onSave={(s) => { openPage(s); }} onCancel={slug ? () => openPage(slug) : goBrowse} />
          )}

          {view === "history" && (
            <div>
              <button onClick={() => slug && openPage(slug)} className="text-sm text-zinc-500 hover:text-zinc-300 mb-4 block">← Back to article</button>
              <h2 className="text-lg font-semibold mb-4">Revision History</h2>
              <div className="space-y-3">
                {revisions.map(r => (
                  <div key={r.version} className="p-3 border border-zinc-800 rounded">
                    <div className="flex justify-between text-sm">
                      <span className="font-semibold">v{r.version}</span>
                      <span className="text-zinc-500">{new Date(r.created_at).toLocaleString()}</span>
                    </div>
                    <p className="text-xs text-zinc-500 mt-1">{r.editor_id} — {r.change_summary}</p>
                  </div>
                ))}
              </div>
            </div>
          )}
        </div>
      </main>
    </div>
  );
}

// ── Editor ──

function Editor({ slug, page, onSave, onCancel }: {
  slug: string; page: Page | null;
  onSave: (s: string) => void; onCancel: () => void;
}) {
  const [title, setTitle] = useState(page?.title ?? "");
  const [content, setContent] = useState(page?.content ?? "");
  const [category, setCategory] = useState(page?.category ?? "");
  const [saving, setSaving] = useState(false);

  const save = async () => {
    if (!title.trim() || !content.trim()) return;
    setSaving(true);
    try {
      const method = slug && page ? "PATCH" : "POST";
      const url = slug && page ? `${API}/pages/${slug}` : `${API}/pages`;
      const body: any = { title: title.trim(), content: content.trim() };
      if (category.trim()) body.category = category.trim();
      const res = await fetch(url, { method, headers: { "content-type": "application/json" }, body: JSON.stringify(body) });
      if (res.ok) { const p = await res.json(); onSave(p.slug); }
    } finally { setSaving(false); }
  };

  return (
    <div>
      <h2 className="text-lg font-semibold mb-4">{slug && page ? "Edit" : "Create"} Article</h2>
      <input value={title} onChange={e => setTitle(e.target.value)} placeholder="Article title" className="w-full bg-zinc-800 px-3 py-2 rounded mb-3 border border-zinc-700 focus:border-blue-500 outline-none text-lg font-semibold" />
      <input value={category} onChange={e => setCategory(e.target.value)} placeholder="Category (optional)" className="w-full bg-zinc-800 px-3 py-2 rounded mb-3 border border-zinc-700 focus:border-blue-500 outline-none text-sm" />
      <textarea value={content} onChange={e => setContent(e.target.value)} placeholder="Write your article in Markdown..." rows={20}
        className="w-full bg-zinc-800 px-3 py-2 rounded mb-3 border border-zinc-700 focus:border-blue-500 outline-none text-sm font-mono leading-relaxed" />
      <div className="flex gap-2">
        <button onClick={save} disabled={saving} className="px-4 py-2 bg-blue-600 rounded hover:bg-blue-500 disabled:opacity-50 text-sm">{saving ? "Saving..." : "Save"}</button>
        <button onClick={onCancel} className="px-4 py-2 bg-zinc-800 rounded hover:bg-zinc-700 text-sm">Cancel</button>
      </div>
      <p className="text-xs text-zinc-600 mt-2">Supports Markdown: # Heading, **bold**, *italic*, `code`, [links](url), - lists</p>
    </div>
  );
}
