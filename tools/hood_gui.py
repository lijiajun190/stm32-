"""Native desktop interface. Serial transactions run in one worker thread."""
from pathlib import Path
import queue
import threading
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

from hood_tool import Client, CONFIG_KEYS, STOP


def run():
    root = tk.Tk()
    root.title("Range Hood Console")
    root.geometry("1040x680")
    root.minsize(850, 520)
    client = None
    busy = False
    closing = False
    messages = queue.Queue()
    port = tk.StringVar(value="COM3")
    notice = tk.StringVar(value="Disconnected")
    progress = tk.DoubleVar(value=0)
    controls = []
    fields = {}

    toolbar = ttk.Frame(root, padding=10)
    toolbar.pack(fill="x")
    ttk.Label(toolbar, text="Port").pack(side="left")
    ports = ttk.Combobox(toolbar, textvariable=port, width=15)
    ports.pack(side="left", padx=8)
    try:
        from serial.tools import list_ports
        ports["values"] = [item.device for item in list_ports.comports()]
    except ImportError:
        pass

    notebook = ttk.Notebook(root)
    notebook.pack(fill="both", expand=True, padx=10)
    status_tab, settings_tab, monitor_tab, upgrade_tab = (ttk.Frame(notebook, padding=12) for _ in range(4))
    for name, tab in zip(("Status", "Settings", "Runtime", "Firmware"), (status_tab, settings_tab, monitor_tab, upgrade_tab)):
        notebook.add(tab, text=name)
    status_view = ttk.Treeview(status_tab, columns=("value",), show="tree headings")
    status_view.heading("#0", text="Measurement")
    status_view.heading("value", text="Value")
    status_view.pack(fill="both", expand=True)
    for row, key in enumerate(CONFIG_KEYS):
        ttk.Label(settings_tab, text=key).grid(row=row, column=0, sticky="w", pady=7)
        variable = tk.StringVar()
        fields[key] = variable
        entry = ttk.Entry(settings_tab, textvariable=variable, width=24)
        entry.grid(row=row, column=1, padx=15)
        controls.append(entry)
    monitor_view = ttk.Treeview(monitor_tab, columns=("cpu", "stack", "period", "jitter", "execution", "overruns"), show="tree headings")
    monitor_view.heading("#0", text="Task")
    monitor_view.column("#0", width=120)
    for key, title in zip(monitor_view["columns"], ("CPU %", "Stack Free B", "Period Max us", "Jitter Max us", "Body Max us", "Overruns")):
        monitor_view.heading(key, text=title)
        monitor_view.column(key, width=120)
    monitor_view.pack(fill="both", expand=True)
    heap_notice = tk.StringVar()
    ttk.Label(monitor_tab, textvariable=heap_notice).pack(anchor="w", pady=8)
    package_path = tk.StringVar()
    package_entry = ttk.Entry(upgrade_tab, textvariable=package_path, width=80)
    package_entry.pack(fill="x", pady=10)
    controls.append(package_entry)
    ttk.Progressbar(upgrade_tab, variable=progress, maximum=100).pack(fill="x", pady=15)
    ttk.Label(root, textvariable=notice, padding=10).pack(fill="x")

    def submit(label, work, kind):
        nonlocal busy
        if busy or closing:
            return
        busy = True
        notice.set(label)
        for widget in controls:
            widget.configure(state="disabled")
        def worker():
            try:
                messages.put((kind, work()))
            except Exception as exc:
                messages.put(("error", str(exc)))
            finally:
                messages.put(("done", None))
        threading.Thread(target=worker, daemon=True).start()

    def connect():
        selected = port.get().strip()
        def work():
            nonlocal client
            if client:
                client.close()
            client = Client(selected)
            return client.info()
        submit("Connecting", work, "info")

    def connected():
        if client:
            return True
        messagebox.showerror("Connection", "Connect a serial port first")
        return False

    def refresh():
        if not connected():
            return
        index = notebook.index(notebook.select())
        work, kind = (client.status, "status") if index == 0 else (client.config, "config") if index == 1 else (client.monitor, "monitor") if index == 2 else (client.info, "info")
        submit("Reading device", work, kind)

    def save():
        if not connected():
            return
        try:
            data = {key: int(value.get()) if key in ("manual_rpm", "max_rpm", "shutdown_delay_ms") else float(value.get()) for key, value in fields.items()}
        except ValueError as exc:
            messagebox.showerror("Settings", str(exc))
            return
        submit("Submitting settings", lambda: client.set_config(data), "submitted")

    def stop():
        if connected():
            submit("Stopping motor", lambda: client.request(STOP), "stopped")

    def select_package():
        chosen = filedialog.askopenfilename(filetypes=(("Upgrade package", "*.hup"), ("All files", "*.*")))
        if chosen:
            package_path.set(chosen)

    def flash():
        if not connected():
            return
        selected = package_path.get()
        if not selected:
            return
        if not messagebox.askyesno("Firmware", "Stop the motor and install the selected firmware package?"):
            return
        try:
            data = Path(selected).read_bytes()
        except OSError as exc:
            messagebox.showerror("Firmware", str(exc))
            return
        progress.set(0)
        submit("Updating firmware", lambda: client.flash(data, lambda count, total: messages.put(("progress", count * 100 / total))), "flashed")

    def button(parent, text, command):
        widget = ttk.Button(parent, text=text, command=command)
        widget.pack(side="left", padx=5)
        controls.append(widget)
    button(toolbar, "Connect", connect)
    button(toolbar, "Refresh", refresh)
    button(toolbar, "Stop", stop)
    settings_actions = ttk.Frame(settings_tab)
    settings_actions.grid(row=8, column=0, columnspan=2, pady=16, sticky="w")
    button(settings_actions, "Apply Settings", save)
    firmware_actions = ttk.Frame(upgrade_tab)
    firmware_actions.pack(fill="x")
    button(firmware_actions, "Select Package", select_package)
    button(firmware_actions, "Install Firmware", flash)

    def poll():
        nonlocal busy
        while True:
            try:
                kind, data = messages.get_nowait()
            except queue.Empty:
                break
            if kind == "done":
                busy = False
                for widget in controls:
                    widget.configure(state="normal")
            elif kind == "progress":
                progress.set(data)
            elif kind in ("status", "info"):
                for item in status_view.get_children():
                    status_view.delete(item)
                for key, value in data.items():
                    status_view.insert("", "end", text=key, values=(value,))
                notice.set("Device read complete")
            elif kind == "config":
                for key, value in data.items():
                    fields[key].set(str(value))
                notice.set("Settings read complete")
            elif kind == "monitor":
                for item in monitor_view.get_children():
                    monitor_view.delete(item)
                for item in data:
                    monitor_view.insert("", "end", text=item["name"], values=tuple(item[key] for key in
                        ("cpu_percent", "stack_free_bytes", "period_max_us", "jitter_max_us", "execution_max_us", "overruns")))
                heap_notice.set(f"Heap free: {data[0]['heap_free_bytes']} B    Minimum: {data[0]['heap_min_bytes']} B")
                notice.set("Runtime counters read complete")
            elif kind == "submitted":
                notice.set("Settings applied; wait for config_saved=1 before power off")
            elif kind == "stopped":
                notice.set("Stop requested")
            elif kind == "flashed":
                notice.set("Application startup confirmed")
                progress.set(100)
            elif kind == "error":
                notice.set(data)
                if not closing:
                    messagebox.showerror("Device", data)
        if closing and not busy:
            if client:
                client.close()
            root.destroy()
            return
        root.after(100, poll)

    def close():
        nonlocal closing
        if busy and not messagebox.askyesno("Close", "Wait for the current operation and close afterwards?"):
            return
        closing = True
        notice.set("Closing after current operation")
    root.protocol("WM_DELETE_WINDOW", close)
    root.after(100, poll)
    root.mainloop()


if __name__ == "__main__":
    run()
