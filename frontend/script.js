const fileInput   = document.getElementById("fileInput");
const dropZone    = document.getElementById("dropZone");
const optimizeBtn = document.getElementById("optimizeBtn");
const statusEl    = document.getElementById("status");
const loader      = document.getElementById("loader");
const fileInfo    = document.getElementById("fileInfo");

let uploadedFileName = null;

// ── Drag & Drop prevention ────────────────────────────────────
window.addEventListener("dragover", e => { e.preventDefault(); e.stopPropagation(); }, false);
window.addEventListener("drop",     e => { e.preventDefault(); e.stopPropagation(); }, false);

dropZone.addEventListener("click", () => fileInput.click());

["dragenter", "dragover", "dragleave", "drop"].forEach(type => {
    dropZone.addEventListener(type, e => {
        e.preventDefault(); e.stopPropagation();
        dropZone.classList.toggle("drop-zone--over", type === "dragover" || type === "dragenter");
    }, false);
});

dropZone.addEventListener("drop", e => {
    const files = e.dataTransfer.files;
    if (files.length > 0) {
        if (files[0].name.endsWith(".cpp")) handleUpload(files[0]);
        else setStatus("❌ Only .cpp files are accepted.", "err");
    }
});

fileInput.addEventListener("change", () => {
    if (fileInput.files.length > 0) handleUpload(fileInput.files[0]);
});

// ── Upload ────────────────────────────────────────────────────
function handleUpload(file) {
    const formData = new FormData();
    formData.append("file", file);

    showLoader(true);
    setStatus("Uploading file to server...");
    optimizeBtn.disabled = true;
    fileInfo.textContent = "";
    hidePanels();

    fetch("http://localhost:3000/upload", { method: "POST", body: formData })
        .then(r => { if (!r.ok) throw new Error("Upload failed"); return r.json(); })
        .then(data => {
            showLoader(false);
            if (data.success) {
                uploadedFileName = data.filename;
                fileInfo.textContent = "📄 " + file.name;
                setStatus("✅ File uploaded — click Run Optimizer", "ok");
                optimizeBtn.disabled = false;
            } else {
                setStatus("❌ Upload failed: " + data.message, "err");
            }
        })
        .catch(err => {
            showLoader(false);
            setStatus("❌ Connection error — is server.js running?", "err");
            console.error(err);
        });
}

// ── Optimize ──────────────────────────────────────────────────
optimizeBtn.addEventListener("click", e => {
    e.preventDefault(); e.stopPropagation();
    if (!uploadedFileName) return;

    showLoader(true);
    setStatus("🔧 Running 8-pass optimizer...");
    hidePanels();
    resetPasses();

    fetch(`http://localhost:3000/optimize?filename=${encodeURIComponent(uploadedFileName)}`)
        .then(r => r.json())
        .then(data => {
            showLoader(false);
            if (data.success) {
                setStatus("✅ Optimization complete! " + data.optimizationLog.length + " transformations applied.", "ok");
                renderResults(data);
            } else {
                setStatus("❌ Compiler error: " + data.message, "err");
            }
        })
        .catch(err => {
            showLoader(false);
            setStatus("❌ Request failed — check server logs.", "err");
            console.error(err);
        });
});

// ── Render Results ────────────────────────────────────────────
function renderResults(data) {
    document.getElementById("originalCode").textContent  = data.originalCode.join("\n");
    document.getElementById("irBefore").textContent      = data.irBefore.join("\n");
    document.getElementById("irAfter").textContent       = data.irAfter.join("\n");
    document.getElementById("optimizedCode").textContent = data.optimizedCode.join("\n");

    renderLog(data.optimizationLog);
    highlightPasses(data.optimizationLog);

    document.getElementById("mainPanels").style.display  = "grid";
    document.getElementById("passesCard").style.display  = "block";
    document.getElementById("logCard").style.display     = "block";
}

// ── Optimization Log ──────────────────────────────────────────
function renderLog(logs) {
    const container = document.getElementById("optimizationLog");
    document.getElementById("logCount").textContent = logs.length + " transformation" + (logs.length !== 1 ? "s" : "");
    container.innerHTML = "";

    if (logs.length === 0) {
        container.innerHTML = '<div class="log-entry log-default">No optimizations applied — code was already optimal.</div>';
        return;
    }

    logs.forEach(entry => {
        const div = document.createElement("div");
        div.className = "log-entry " + getLogClass(entry);
        div.textContent = entry;
        container.appendChild(div);
    });
}

function getLogClass(entry) {
    if (entry.includes("[ConstFold"))  return "log-constfold";
    if (entry.includes("[ConstProp"))  return "log-constprop";
    if (entry.includes("[CopyProp"))   return "log-copyprop";
    if (entry.includes("[AlgSimp"))    return "log-algsimp";
    if (entry.includes("[StrRed"))     return "log-strred";
    if (entry.includes("[CSE"))        return "log-cse";
    if (entry.includes("[LoopInv"))    return "log-loopinv";
    if (entry.includes("[DCE"))        return "log-dce";
    return "log-default";
}

// ── Highlight Active Passes ───────────────────────────────────
const passMap = {
    "[ConstFold": "pass-constfold",
    "[ConstProp": "pass-constprop",
    "[CopyProp":  "pass-copyprop",
    "[AlgSimp":   "pass-algsimp",
    "[StrRed":    "pass-strred",
    "[CSE":       "pass-cse",
    "[LoopInv":   "pass-loopinv",
    "[DCE":       "pass-dce"
};

function highlightPasses(logs) {
    document.getElementById("passesCard").style.display = "block";
    logs.forEach(entry => {
        for (const [key, id] of Object.entries(passMap)) {
            if (entry.includes(key)) {
                const el = document.getElementById(id);
                if (el) el.classList.add("active");
            }
        }
    });
}

function resetPasses() {
    Object.values(passMap).forEach(id => {
        const el = document.getElementById(id);
        if (el) el.classList.remove("active");
    });
}

// ── Helpers ───────────────────────────────────────────────────
function setStatus(msg, cls) {
    statusEl.textContent = msg;
    statusEl.parentElement.className = "status-bar" + (cls ? " " + cls : "");
}

function showLoader(show) {
    loader.style.display = show ? "block" : "none";
}

function hidePanels() {
    document.getElementById("mainPanels").style.display = "none";
    document.getElementById("logCard").style.display    = "none";
}
