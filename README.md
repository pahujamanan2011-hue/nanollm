# ⚡ nanollm

A native, ultra-lightweight C/GTK3 desktop text editor and AI studio inspired by Geany. Built specifically to eliminate browser and Electron memory bloat, keeping idle memory footprint **under 15 MB RAM** while providing native multi-provider LLM integration.

---

## 🌟 Key Features

* **Sub-15 MB RAM Footprint:** Written in native C with GTK3—no browser engine or heavy runtimes.
* **Multi-Provider LLM Integration:** Built-in native support for **Groq**, **Google Gemini**, and **Hugging Face**.
* **Geany-Inspired UI:** Split-pane interface featuring prompt history, continuous response view, input area, and model options.
* **Non-Blocking Architecture:** Asynchronous API calls via `libcurl` and `pthreads` ensure the interface remains fluid.
* **Persistent History & Config:** Automatically saves settings and chat sessions as local JSON files in `~/.config/nanollm_editor/`.

---

## 🚀 Quick Install (Automated)

Run the included automated setup script to detect your OS, install dependencies, and build the project automatically:

```bash
git clone [https://github.com/your-username/nanollm.git](https://github.com/your-username/nanollm.git)
cd nanollm
chmod +x install.sh
./install.sh

```
## 🛠️ Step-by-Step Manual Installation
If you prefer to set up and compile the project manually, follow these steps:
### Step 1: Clone the Repository
Open your terminal and clone the source code:
```bash
git clone [https://github.com/your-username/nanollm.git](https://github.com/your-username/nanollm.git)
cd nanollm

```
### Step 2: Install Required System Dependencies
Install a C compiler (gcc), make, gtk3, libcurl, and cjson for your specific operating system:
 * **Ubuntu / Debian / Linux Mint:**
   ```bash
   sudo apt update
   sudo apt install -y build-essential libgtk-3-dev libcurl4-openssl-dev libcjson-dev
   
   ```
 * **Arch Linux / Manjaro:**
   ```bash
   sudo pacman -Sy --needed base-devel gtk3 curl cjson
   
   ```
 * **Fedora / RHEL / CentOS:**
   ```bash
   sudo dnf install -y gcc make gtk3-devel libcurl-devel cjson-devel
   
   ```
 * **macOS (via Homebrew):**
   ```bash
   brew install gcc make gtk+3 cjson curl pkg-config
   
   ```
 * **Windows (via MSYS2 UCRT64 Terminal):**
   ```bash
   pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make mingw-w64-ucrt-x86_64-gtk3 mingw-w64-ucrt-x86_64-curl mingw-w64-ucrt-x86_64-cjson
   
   ```
### Step 3: Build the Binary
Compile the project using make:
 * **Linux / macOS:**
   ```bash
   make
   
   ```
 * **Windows (MSYS2):**
   ```bash
   mingw32-make
   
   ```
### Step 4: Run the Application
Start the editor directly from your terminal:
 * **Linux / macOS:**
   ```bash
   ./nanollm_editor
   
   ```
 * **Windows:**
   ```cmd
   nanollm_editor.exe
   
   ```
## 💡 Quick Start Guide
 1. **Configure API Keys:** Launch the application and open **Edit → API Keys...** (or click *Configure Keys* in the sidebar).
 2. **Select Provider:** Choose between **Groq**, **Gemini**, or **Hugging Face** from the sidebar dropdown.
 3. **Prompting:** Enter your text prompt in the bottom text pane and press **Ctrl + Enter** or click **Send**.
 4. **Chat History:** Click any session in the sidebar list to instantly reload past conversation threads.
## 📄 License
This project is licensed under the **MIT License** — see the LICENSE file for details.
