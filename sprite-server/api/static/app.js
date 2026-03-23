/* DeskPet Sprite Server — frontend */

const API = "";  // same origin

// ---------------------------------------------------------------------------
// Elements
// ---------------------------------------------------------------------------
const manifestBadge = document.getElementById("manifest-badge");
const fileInput     = document.getElementById("file-input");
const previewImg    = document.getElementById("preview-img");
const btnPreview    = document.getElementById("btn-preview");
const btnDownload   = document.getElementById("btn-download");
const btnPublish    = document.getElementById("btn-publish");
const resultBox     = document.getElementById("result-box");
const publishForm   = document.getElementById("publish-form");
const library       = document.getElementById("library");
const btnRefresh    = document.getElementById("btn-refresh");

// ---------------------------------------------------------------------------
// Manifest badge
// ---------------------------------------------------------------------------
async function loadManifest() {
  try {
    const res = await fetch(`${API}/manifest`);
    const m   = await res.json();
    manifestBadge.textContent = `v${m.version}`;
  } catch {
    manifestBadge.textContent = "v?";
  }
}

// ---------------------------------------------------------------------------
// Library
// ---------------------------------------------------------------------------
async function loadLibrary() {
  try {
    const res  = await fetch(`${API}/published`);
    const data = await res.json();
    renderLibrary(data);
  } catch (e) {
    library.innerHTML = `<p class="empty-note">Could not load library: ${e.message}</p>`;
  }
}

function renderLibrary(data) {
  const chars = Object.keys(data);
  if (chars.length === 0) {
    library.innerHTML = `<p class="empty-note">No sprites published yet.</p>`;
    return;
  }
  library.innerHTML = chars.map(char => `
    <div class="char-group">
      <div class="char-name">${char}</div>
      <div class="expr-list">
        ${data[char].map(e => `<span class="expr-chip">${e}</span>`).join("")}
      </div>
    </div>
  `).join("");
}

// ---------------------------------------------------------------------------
// File selection → preview
// ---------------------------------------------------------------------------
fileInput.addEventListener("change", () => {
  const file = fileInput.files[0];
  if (!file) return;

  btnPreview.disabled = false;
  btnPublish.disabled = false;
  btnDownload.hidden  = true;  // new file selected — previous download no longer relevant
  hideResult();

  const url = URL.createObjectURL(file);
  previewImg.src     = url;
  previewImg.hidden  = false;
});

// ---------------------------------------------------------------------------
// Preview (convert without publishing)
// ---------------------------------------------------------------------------
btnPreview.addEventListener("click", async () => {
  const file = fileInput.files[0];
  if (!file) return;

  showResult("info", "Converting…");

  const fd = new FormData();
  fd.append("file", file);

  try {
    const res  = await fetch(`${API}/convert`, { method: "POST", body: fd });
    const data = await res.json();
    if (!res.ok) throw new Error(data.detail || res.statusText);
    showResult("info",
      `File:    ${data.filename}\n` +
      `Frames:  ${data.frames}\n` +
      `Size:    ${(data.sprite_bytes / 1024).toFixed(1)} KB total  (${data.frame_bytes} B / frame)`
    );
  } catch (e) {
    showResult("error", `Preview failed:\n${e.message}`);
  }
});

// ---------------------------------------------------------------------------
// Publish
// ---------------------------------------------------------------------------
publishForm.addEventListener("submit", async (e) => {
  e.preventDefault();

  const file       = fileInput.files[0];
  const character  = document.getElementById("character").value.trim();
  const expression = document.getElementById("expression").value.trim();

  if (!file || !character || !expression) return;

  btnPublish.disabled = true;
  btnDownload.hidden  = true;
  showResult("info", "Publishing…");

  const fd = new FormData();
  fd.append("file",       file);
  fd.append("character",  character);
  fd.append("expression", expression);

  try {
    const res  = await fetch(`${API}/publish`, { method: "POST", body: fd });
    const data = await res.json();
    if (!res.ok) throw new Error(data.detail || res.statusText);

    showResult("ok",
      `Published!  ${data.path}\n` +
      `Frames:     ${data.frames}\n` +
      `Size:       ${(data.sprite_bytes / 1024).toFixed(1)} KB\n` +
      `Manifest:   v${data.manifest_version}`
    );

    // Show the Download button pointing at the file we just published
    btnDownload.hidden = false;
    btnDownload.onclick = () => {
      window.location.href = `${API}/download/${encodeURIComponent(data.character)}/${encodeURIComponent(data.expression)}`;
    };

    await Promise.all([loadManifest(), loadLibrary()]);
  } catch (e) {
    showResult("error", `Publish failed:\n${e.message}`);
  } finally {
    btnPublish.disabled = false;
  }
});

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
function showResult(type, text) {
  resultBox.className = type;
  resultBox.textContent = text;
  resultBox.hidden = false;
}

function hideResult() {
  resultBox.hidden = true;
  resultBox.textContent = "";
}

// ---------------------------------------------------------------------------
// Refresh button
// ---------------------------------------------------------------------------
btnRefresh.addEventListener("click", () => loadLibrary());

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------
loadManifest();
loadLibrary();
