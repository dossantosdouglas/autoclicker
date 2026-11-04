"""
Backend de automação de cliques para Linux (GNOME Wayland e X11).
Utiliza libei + Mutter EIS para controle nativo no GNOME Wayland (sem root)
e python-xlib como fallback no X11.
Suporta persistência e salvamento automático de configurações em ~/.config/autoclicker/.
"""

import os
import sys
import time
import json
import subprocess
import threading
from dataclasses import dataclass
from typing import Callable, Optional, List, Tuple

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
EI_CLICKER_BIN = os.path.join(SCRIPT_DIR, "ei_clicker")
CONFIG_DIR = os.path.expanduser("~/.config/autoclicker")
CONFIG_FILE = os.path.join(CONFIG_DIR, "config.json")

# Constantes de botão evdev / libei
EVDEV_BTN_LEFT = 272    # 0x110
EVDEV_BTN_RIGHT = 273   # 0x111
EVDEV_BTN_MIDDLE = 274  # 0x112

# Constantes X11
X11_BTN_LEFT = 1
X11_BTN_MIDDLE = 2
X11_BTN_RIGHT = 3


def get_pointer_position() -> Tuple[int, int]:
    """Retorna as coordenadas atuais (X, Y) do cursor via Xlib/Xwayland se disponível."""
    try:
        from Xlib import display
        d = display.Display()
        qp = d.screen().root.query_pointer()
        pos = (int(qp.root_x), int(qp.root_y))
        d.close()
        return pos
    except Exception:
        return (0, 0)


class EIClicker:
    """Emulador de alta precisão via libei e interface EIS do Mutter (GNOME Wayland)."""

    def __init__(self):
        import dbus
        self._dbus = dbus
        self._bus = dbus.SessionBus()
        self._mutter_rd = self._bus.get_object(
            'org.gnome.Mutter.RemoteDesktop',
            '/org/gnome/Mutter/RemoteDesktop'
        )
        self.session_path = None
        self.session_iface = None
        self.bin_path = EI_CLICKER_BIN
        self._ensure_session()

    def _ensure_session(self):
        if self.session_iface is None:
            self.session_path = self._mutter_rd.CreateSession(
                dbus_interface='org.gnome.Mutter.RemoteDesktop'
            )
            session_obj = self._bus.get_object('org.gnome.Mutter.RemoteDesktop', self.session_path)
            self.session_iface = self._dbus.Interface(session_obj, 'org.gnome.Mutter.RemoteDesktop.Session')
            self.session_iface.Start()

    def click(self, x=None, y=None, button="left", double=False, restore_x=None, restore_y=None):
        self._ensure_session()

        btn_map = {"left": EVDEV_BTN_LEFT, "right": EVDEV_BTN_RIGHT, "middle": EVDEV_BTN_MIDDLE}
        btn_code = btn_map.get(button.lower(), EVDEV_BTN_LEFT)
        d_val = 1 if double else 0

        tx = int(x) if x is not None and x >= 0 else -1
        ty = int(y) if y is not None and y >= 0 else -1
        rx = int(restore_x) if restore_x is not None and restore_x >= 0 else -1
        ry = int(restore_y) if restore_y is not None and restore_y >= 0 else -1

        try:
            fd_obj = self.session_iface.ConnectToEIS({})
            fd = fd_obj.take()
        except Exception:
            self.session_iface = None
            self._ensure_session()
            fd_obj = self.session_iface.ConnectToEIS({})
            fd = fd_obj.take()

        cmd = [
            self.bin_path,
            str(fd),
            str(tx),
            str(ty),
            str(btn_code),
            str(d_val),
            str(rx),
            str(ry)
        ]
        res = subprocess.run(cmd, pass_fds=[fd], stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=4)
        if res.returncode != 0:
            raise RuntimeError(f"Erro no ei_clicker: {res.stderr.decode('utf-8', errors='ignore')}")

    def close(self):
        if self.session_iface:
            try:
                self.session_iface.Stop()
            except Exception:
                pass
            self.session_iface = None


class X11Clicker:
    """Emulador de cliques tradicional usando XTest e python-xlib (compatível com X11)."""

    def __init__(self):
        from Xlib import X, display
        from Xlib.ext import xtest
        self.X = X
        self.display = display.Display()
        self.xtest = xtest

    def click(self, x=None, y=None, button="left", double=False, restore_x=None, restore_y=None):
        root = self.display.screen().root

        if x is not None and y is not None and x >= 0 and y >= 0:
            root.warp_pointer(int(x), int(y))
            self.display.sync()
            time.sleep(0.04)

        btn_map = {"left": X11_BTN_LEFT, "middle": X11_BTN_MIDDLE, "right": X11_BTN_RIGHT}
        code = btn_map.get(button.lower(), X11_BTN_LEFT)
        clicks = 2 if double else 1

        for i in range(clicks):
            self.xtest.fake_input(self.display, self.X.ButtonPress, code)
            self.xtest.fake_input(self.display, self.X.ButtonRelease, code)
            self.display.sync()
            if i < clicks - 1:
                time.sleep(0.08)

        if restore_x is not None and restore_y is not None and restore_x >= 0 and restore_y >= 0:
            time.sleep(0.04)
            root.warp_pointer(int(restore_x), int(restore_y))
            self.display.sync()

    def close(self):
        try:
            self.display.close()
        except Exception:
            pass


def get_clicker():
    """Detecta e retorna o emulador adequado para o ambiente atual."""
    if os.path.exists(EI_CLICKER_BIN) and os.environ.get("XDG_CURRENT_DESKTOP") in ("GNOME", "ubuntu:GNOME", "pop:GNOME"):
        try:
            return EIClicker()
        except Exception as e:
            print(f"[Aviso] Falha ao iniciar EIClicker ({e}), usando fallback.")

    try:
        return X11Clicker()
    except Exception as e:
        print(f"[Aviso] Falha ao iniciar X11Clicker ({e}).")

    try:
        return EIClicker()
    except Exception as e:
        raise RuntimeError(f"Não foi possível inicializar nenhum emulador de clique: {e}")


def play_feedback_sound():
    """Emite um som sutil de feedback a cada clique."""
    def _play():
        try:
            subprocess.run(
                ["canberra-gtk-play", "-i", "button-pressed"],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=1
            )
        except Exception:
            try:
                sys.stdout.write('\a')
                sys.stdout.flush()
            except Exception:
                pass

    threading.Thread(target=_play, daemon=True).start()


@dataclass
class ClickTarget:
    """Configuração e estado de um ponto de clique específico."""
    id: int
    name: str = "Ponto 1"
    enabled: bool = True
    use_fixed_pos: bool = True
    x: Optional[int] = None
    y: Optional[int] = None
    minutes: float = 2.0
    seconds: float = 0.0
    button: str = "left"          # left, right, middle
    double_click: bool = False

    # Estado de execução
    remaining_seconds: float = 0.0
    total_clicks: int = 0

    @property
    def interval_seconds(self) -> float:
        tot = (self.minutes * 60.0) + self.seconds
        return max(0.2, tot)

    def reset_timer(self):
        self.remaining_seconds = self.interval_seconds


class MultiClickEngine:
    """Motor de execução que gerencia cliques em múltiplos pontos independentes."""

    def __init__(self, clicker=None):
        self._clicker = clicker
        self.targets: List[ClickTarget] = [
            ClickTarget(id=1, name="Ponto 1 (Tempo X)", minutes=2.0, seconds=0.0, x=1920, y=500, use_fixed_pos=True),
            ClickTarget(id=2, name="Ponto 2 (Tempo Y)", minutes=5.0, seconds=0.0, x=500, y=500, use_fixed_pos=True)
        ]
        self.play_sound: bool = True
        self.is_running: bool = False

        self._thread: Optional[threading.Thread] = None
        self._stop_event = threading.Event()
        self._lock = threading.Lock()

        # Callbacks
        self.on_tick: Optional[Callable[[List[ClickTarget]], None]] = None
        self.on_target_click: Optional[Callable[[ClickTarget, str], None]] = None
        self.on_status_change: Optional[Callable[[bool], None]] = None

    def _ensure_clicker(self):
        if self._clicker is None:
            self._clicker = get_clicker()

    def get_target(self, target_id: int) -> Optional[ClickTarget]:
        for t in self.targets:
            if t.id == target_id:
                return t
        return None

    def start(self):
        with self._lock:
            if self.is_running:
                return
            self._ensure_clicker()
            self.is_running = True
            for t in self.targets:
                t.reset_timer()
            self._stop_event.clear()
            self._thread = threading.Thread(target=self._run_loop, daemon=True)
            self._thread.start()

        if self.on_status_change:
            self.on_status_change(True)

    def stop(self):
        with self._lock:
            if not self.is_running:
                return
            self.is_running = False
            self._stop_event.set()

        if self.on_status_change:
            self.on_status_change(False)

    def trigger_target_now(self, target_id: int):
        target = self.get_target(target_id)
        if not target:
            return
        self._execute_click_on_target(target)

    def _execute_click_on_target(self, target: ClickTarget):
        self._ensure_clicker()

        tx = target.x if target.use_fixed_pos else None
        ty = target.y if target.use_fixed_pos else None

        self._clicker.click(
            x=tx,
            y=ty,
            button=target.button,
            double=target.double_click
        )

        target.total_clicks += 1

        if self.play_sound:
            play_feedback_sound()

        if self.on_target_click:
            timestamp = time.strftime("%H:%M:%S")
            self.on_target_click(target, timestamp)

    def _run_loop(self):
        last_check = time.time()
        while not self._stop_event.is_set():
            time.sleep(0.15)
            now = time.time()
            dt = now - last_check
            last_check = now

            with self._lock:
                if not self.is_running:
                    break

                for target in self.targets:
                    if not target.enabled:
                        continue

                    target.remaining_seconds -= dt

                    if target.remaining_seconds <= 0:
                        try:
                            self._execute_click_on_target(target)
                        except Exception as e:
                            print(f"[Erro ao clicar no {target.name}] {e}")
                        target.reset_timer()

            if self.on_tick:
                self.on_tick(self.targets)

    def close(self):
        self.stop()
        if self._clicker:
            try:
                self._clicker.close()
            except Exception:
                pass
            self._clicker = None


def save_settings(engine: MultiClickEngine, file_path: Optional[str] = None) -> bool:
    """Salva as configurações do aplicativo em arquivo JSON."""
    if file_path is None:
        file_path = CONFIG_FILE
    try:
        os.makedirs(os.path.dirname(file_path), exist_ok=True)
        data = {
            "version": 2,
            "play_sound": engine.play_sound,
            "targets": []
        }
        for t in engine.targets:
            data["targets"].append({
                "id": t.id,
                "name": t.name,
                "enabled": t.enabled,
                "use_fixed_pos": t.use_fixed_pos,
                "x": t.x,
                "y": t.y,
                "minutes": t.minutes,
                "seconds": t.seconds,
                "button": t.button,
                "double_click": t.double_click
            })

        tmp_path = file_path + ".tmp"
        with open(tmp_path, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2, ensure_ascii=False)
        os.replace(tmp_path, file_path)
        return True
    except Exception as e:
        print(f"[Erro ao salvar configurações] {e}")
        return False


def load_settings(engine: MultiClickEngine, file_path: Optional[str] = None) -> bool:
    """Carrega as configurações salvas em arquivo JSON para o motor."""
    if file_path is None:
        file_path = CONFIG_FILE
    if not os.path.exists(file_path):
        return False
    try:
        with open(file_path, "r", encoding="utf-8") as f:
            data = json.load(f)

        engine.play_sound = data.get("play_sound", True)
        targets_data = data.get("targets", [])
        for td in targets_data:
            tid = td.get("id")
            target = engine.get_target(tid)
            if target:
                target.enabled = td.get("enabled", True)
                target.use_fixed_pos = td.get("use_fixed_pos", True)
                target.x = td.get("x")
                target.y = td.get("y")
                target.minutes = float(td.get("minutes", 2.0))
                target.seconds = float(td.get("seconds", 0.0))
                target.button = td.get("button", "left")
                target.double_click = td.get("double_click", False)
                target.reset_timer()
        return True
    except Exception as e:
        print(f"[Erro ao carregar configurações] {e}")
        return False


# Atalho compatível com versão anterior
class ClickEngine(MultiClickEngine):
    def __init__(self, clicker=None):
        super().__init__(clicker)
        self.target1 = self.targets[0]

    @property
    def interval_seconds(self) -> float:
        return self.target1.interval_seconds

    @interval_seconds.setter
    def interval_seconds(self, val: float):
        self.target1.minutes = val / 60.0
        self.target1.seconds = 0.0

    @property
    def button(self) -> str:
        return self.target1.button

    @button.setter
    def button(self, val: str):
        self.target1.button = val

    @property
    def double_click(self) -> bool:
        return self.target1.double_click

    @double_click.setter
    def double_click(self, val: bool):
        self.target1.double_click = val

    @property
    def remaining_seconds(self) -> float:
        return self.target1.remaining_seconds

    @property
    def total_clicks(self) -> int:
        return self.target1.total_clicks

    def set_interval(self, minutes: float = 0, seconds: float = 0):
        self.target1.minutes = minutes
        self.target1.seconds = seconds
        self.target1.reset_timer()

    def trigger_now(self):
        self.trigger_target_now(1)
