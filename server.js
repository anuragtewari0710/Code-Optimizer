const express = require("express");
const multer = require("multer");
const fs = require("fs");
const path = require("path");
const { exec } = require("child_process");
const cors = require("cors");

const app = express();
const PORT = 3000;

app.use(cors());
app.use(express.json());

// Absolute Paths
const uploadDir  = path.resolve(__dirname, "upload");
const tempDir    = path.resolve(__dirname, "..", "temp");
const compilerDir = path.resolve(__dirname, "..", "compiler");

[uploadDir, tempDir].forEach(dir => {
    if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
});

const storage = multer.diskStorage({
    destination: (req, file, cb) => cb(null, uploadDir),
    filename:    (req, file, cb) => cb(null, Date.now() + "-" + file.originalname)
});
const upload = multer({ storage });

// ── 1. Upload Route ──────────────────────────────────────────
app.post("/upload", upload.single("file"), (req, res) => {
    if (!req.file) return res.status(400).json({ success: false, message: "No file uploaded." });
    res.json({ success: true, filename: req.file.filename, originalName: req.file.originalname });
});

// ── 2. Optimize Route ────────────────────────────────────────
app.get("/optimize", (req, res) => {
    const filename = req.query.filename;
    if (!filename) return res.json({ success: false, message: "No filename provided" });

    const inputPath  = path.join(uploadDir, filename);
    const tempInput  = path.join(tempDir, "input.cpp");
    const tempOutput = path.join(tempDir, "output.json");  // now JSON

    try {
        fs.copyFileSync(inputPath, tempInput);
    } catch (err) {
        return res.json({ success: false, message: "FileSystem error: " + err.message });
    }

    // Compile the optimizer first, then run it
    const compileCmd = `g++ -std=c++17 main.cpp lexer.cpp parser.cpp icg.cpp optimizer.cpp -o compiler_exec`;
    const inputAbs  = tempInput.replace(/\\/g, "/");
    const outputAbs = tempOutput.replace(/\\/g, "/");
    const runCmd    = process.platform === "win32"
        ? `compiler_exec.exe "${inputAbs}" "${outputAbs}"`
        : `./compiler_exec "${inputAbs}" "${outputAbs}"`;

    exec(`${compileCmd} && ${runCmd}`, { cwd: compilerDir }, (err, stdout, stderr) => {
        if (err) {
            console.error("Exec error:", stderr);
            return res.json({ success: false, message: stderr || "Optimization failed" });
        }

        try {
            const raw = fs.readFileSync(tempOutput, "utf-8");
            const data = JSON.parse(raw);

            // Also read original source for display
            const originalCode = fs.readFileSync(tempInput, "utf-8").split("\n");

            res.json({
                success: true,
                originalCode:     originalCode,
                optimizedCode:    data.optimizedCode,
                irBefore:         data.irBefore,
                irAfter:          data.irAfter,
                optimizationLog:  data.optimizationLog
            });
        } catch (e) {
            res.json({ success: false, message: "Failed to parse output: " + e.message });
        }
    });
});

app.listen(PORT, () => console.log(`\n✅  Server running at http://localhost:${PORT}\n`));
