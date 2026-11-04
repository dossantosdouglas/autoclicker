"""
Interface gráfica moderna (GTK 4 + Libadwaita) para o AutoClicker Multi-Ponto.
Suporte completo a múltiplos monitores, seletor visual na tela (crosshairs),
e persistência/salvamento automático de configurações (config.json).
"""

import sys
import time
import os
import gi

gi.require_version('Gtk', '4.0')
gi.require_version('Adw', '1')
from gi.repository import Gtk, Adw, GLib, Gio, Pango, Gdk

from backend import MultiClickEngine, ClickTarget, save_settings, load_settings, CONFIG_FILE


def get_monitor_info(x: int, y: int) -> str:
    """Identifica em qual monitor as coordenadas (X, Y) estão localizadas."""
    disp = Gdk.Display.get_default()
    if not disp:
        return f"Coordenadas: ({x}, {y})"

    monitors = disp.get_monitors()
    for i in range(monitors.get_n_items()):
        m = monitors.get_item(i)
        g = m.get_geometry()
        if (g.x <= x < g.x + g.width) and (g.y <= y < g.y + g.height):
            desc = m.get_description() or f"Monitor {i+1}"
            return f"{desc} | Global ({x}, {y})"

    return f"Global: ({x}, {y})"


class MultiScreenPicker:
    """Gerencia overlays em todos os monitores simultaneamente para captura de clique."""

    def __init__(self, target_name: str, on_picked_callback):
        self.target_name = target_name
        self.callback = on_picked_callback
        self.windows = []

        disp = Gdk.Display.get_default()
        monitors = disp.get_monitors()

        css_provider = Gtk.CssProvider()
        css_provider.load_from_data(b"""
            window.picker-window {
                background-color: rgba(12, 18, 32, 0.45);
            }
            .picker-card {
                background-color: rgba(25, 28, 36, 0.94);
                color: #ffffff;
                border-radius: 18px;
                padding: 20px 28px;
                box-shadow: 0 10px 30px rgba(0,0,0,0.6);
                border: 2px solid #3584e4;
            }
        """)
        Gtk.StyleContext.add_provider_for_display(
            disp,
            css_provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )

        cursor = Gdk.Cursor.new_from_name("crosshair", None)

        for i in range(monitors.get_n_items()):
            m = monitors.get_item(i)
            geom = m.get_geometry()
            desc = m.get_description() or f"Monitor {i+1}"

            win = Gtk.Window()
            win.set_title(f"Seletor - {desc}")
            win.fullscreen_on_monitor(m)
            win.set_cursor(cursor)
            win.add_css_class("picker-window")

            box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
            box.set_valign(Gtk.Align.CENTER)
            box.set_halign(Gtk.Align.CENTER)

            card = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
            card.add_css_class("picker-card")

            lbl_icon = Gtk.Label(label="🎯")
            attr_icon = Pango.AttrList()
            attr_icon.insert(Pango.attr_scale_new(2.4))
            lbl_icon.set_attributes(attr_icon)
            card.append(lbl_icon)

            lbl_mon = Gtk.Label(label=f"{desc} (Resolução: {geom.width}x{geom.height})")
            lbl_mon.add_css_class("caption")
            lbl_mon.add_css_class("accent")
            card.append(lbl_mon)

            lbl_title = Gtk.Label(label=f"Definindo posição do {self.target_name}")
            lbl_title.add_css_class("title-2")
            card.append(lbl_title)

            lbl_sub = Gtk.Label(
                label="Clique com o mouse exatamente onde você quer o clique nesta tela.\n(Pressione ESC em qualquer monitor para cancelar)"
            )
            lbl_sub.set_justify(Gtk.Justification.CENTER)
            lbl_sub.add_css_class("dim-label")
            card.append(lbl_sub)

            box.append(card)
            win.set_child(box)

            click_gesture = Gtk.GestureClick.new()
            click_gesture.connect("pressed", lambda g, n, lx, ly, gm=geom: self._on_clicked(gm, lx, ly))
            win.add_controller(click_gesture)

            key_ctrl = Gtk.EventControllerKey.new()
            key_ctrl.connect("key-pressed", self._on_key_pressed)
            win.add_controller(key_ctrl)

            win.present()
            self.windows.append(win)

    def _on_clicked(self, geom, local_x, local_y):
        global_x = geom.x + int(local_x)
        global_y = geom.y + int(local_y)
        self.close_all()
        if self.callback:
            self.callback(global_x, global_y)

    def _on_key_pressed(self, controller, keyval, keycode, state):
        if keyval == Gdk.KEY_Escape:
            self.close_all()
            return True
        return False

    def close_all(self):
        for w in self.windows:
            w.destroy()
        self.windows.clear()


class TargetCardWidget(Gtk.Box):
    """Componente visual que gerencia um Ponto de Clique."""

    def __init__(self, target: ClickTarget, engine: MultiClickEngine, parent_win):
        super().__init__(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        self.target = target
        self.engine = engine
        self.parent_win = parent_win

        self.add_css_class("card")
        self.set_margin_top(4)
        self.set_margin_bottom(8)

        self._build_ui()
        self.load_from_target()

    def _build_ui(self):
        inner = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=12)
        inner.set_margin_top(16)
        inner.set_margin_bottom(16)
        inner.set_margin_start(16)
        inner.set_margin_end(16)
        self.append(inner)

        # -------------------------------------------------------------
        # Cabeçalho: Título e Interruptor Liga/Desliga
        # -------------------------------------------------------------
        header_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)

        title_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        icon = Gtk.Image.new_from_icon_name("input-mouse-symbolic")
        title_box.append(icon)

        self.lbl_title = Gtk.Label(label=self.target.name)
        self.lbl_title.add_css_class("title-3")
        title_box.append(self.lbl_title)
        title_box.set_hexpand(True)
        title_box.set_halign(Gtk.Align.START)
        header_box.append(title_box)

        self.switch_enabled = Gtk.Switch()
        self.switch_enabled.set_active(self.target.enabled)
        self.switch_enabled.set_valign(Gtk.Align.CENTER)
        self.switch_enabled.connect("notify::active", self._on_enabled_toggled)
        header_box.append(self.switch_enabled)

        inner.append(header_box)

        # -------------------------------------------------------------
        # Temporizador e Progresso
        # -------------------------------------------------------------
        status_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        self.lbl_status = Gtk.Label(label="Parado")
        self.lbl_status.add_css_class("dim-label")
        self.lbl_status.add_css_class("caption")
        self.lbl_status.set_hexpand(True)
        self.lbl_status.set_halign(Gtk.Align.START)
        status_box.append(self.lbl_status)

        self.lbl_clicks = Gtk.Label(label="0 cliques")
        self.lbl_clicks.add_css_class("accent")
        self.lbl_clicks.add_css_class("caption")
        status_box.append(self.lbl_clicks)
        inner.append(status_box)

        self.lbl_countdown = Gtk.Label(label="00:00")
        attr_list = Pango.AttrList()
        attr_list.insert(Pango.attr_scale_new(2.2))
        attr_list.insert(Pango.attr_weight_new(Pango.Weight.BOLD))
        self.lbl_countdown.set_attributes(attr_list)
        inner.append(self.lbl_countdown)

        self.progress_bar = Gtk.ProgressBar()
        self.progress_bar.set_fraction(1.0)
        inner.append(self.progress_bar)

        sep = Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL)
        sep.set_margin_top(4)
        sep.set_margin_bottom(4)
        inner.append(sep)

        # -------------------------------------------------------------
        # Configuração de Local / Coordenadas (Suporte Multi-Monitor)
        # -------------------------------------------------------------
        pos_group = Adw.PreferencesGroup(title="Local do Clique (Multi-Monitor)")

        self.switch_fixed = Adw.SwitchRow(
            title="Usar Posição Fixa na Tela",
            subtitle="Mover o mouse para coordenadas específicas antes de clicar"
        )
        self.switch_fixed.set_active(self.target.use_fixed_pos)
        self.switch_fixed.connect("notify::active", self._on_fixed_toggled)
        pos_group.add(self.switch_fixed)

        self.row_select = Adw.ActionRow(
            title="Selecionar Alvo Visualmente",
            subtitle="Clique em 'Marcar' para abrir a mira em todos os monitores"
        )
        btn_pick = Gtk.Button()
        btn_pick.add_css_class("suggested-action")
        pick_content = Adw.ButtonContent(
            icon_name="find-location-symbolic",
            label="Marcar na Tela"
        )
        btn_pick.set_child(pick_content)
        btn_pick.connect("clicked", self._on_pick_clicked)
        self.row_select.add_suffix(btn_pick)
        pos_group.add(self.row_select)

        # Exibição do Monitor detectado
        self.row_monitor_info = Adw.ActionRow(
            title="Monitor Selecionado",
            subtitle=get_monitor_info(self.target.x or 0, self.target.y or 0)
        )
        pos_group.add(self.row_monitor_info)

        # Coordenada X manual
        self.spin_x = Adw.SpinRow.new_with_range(0, 10000, 10)
        self.spin_x.set_title("Coordenada Global X (pixels)")
        self.spin_x.set_value(self.target.x if self.target.x is not None else 1920)
        self.spin_x.connect("notify::value", lambda *a: self.sync_from_ui())
        pos_group.add(self.spin_x)

        # Coordenada Y manual
        self.spin_y = Adw.SpinRow.new_with_range(0, 6000, 10)
        self.spin_y.set_title("Coordenada Global Y (pixels)")
        self.spin_y.set_value(self.target.y if self.target.y is not None else 500)
        self.spin_y.connect("notify::value", lambda *a: self.sync_from_ui())
        pos_group.add(self.spin_y)

        inner.append(pos_group)

        # -------------------------------------------------------------
        # Configuração de Intervalo de Tempo
        # -------------------------------------------------------------
        time_group = Adw.PreferencesGroup(title="Intervalo de Tempo")

        self.spin_minutes = Adw.SpinRow.new_with_range(0, 1440, 1)
        self.spin_minutes.set_title("Minutos")
        self.spin_minutes.set_value(self.target.minutes)
        self.spin_minutes.connect("notify::value", lambda *a: self.sync_from_ui())
        time_group.add(self.spin_minutes)

        self.spin_seconds = Adw.SpinRow.new_with_range(0, 59, 1)
        self.spin_seconds.set_title("Segundos adicionais")
        self.spin_seconds.set_value(self.target.seconds)
        self.spin_seconds.connect("notify::value", lambda *a: self.sync_from_ui())
        time_group.add(self.spin_seconds)

        inner.append(time_group)

        # Atalhos rápidos de tempo
        presets_flow = Gtk.FlowBox()
        presets_flow.set_selection_mode(Gtk.SelectionMode.NONE)
        presets_flow.set_max_children_per_line(6)
        presets_flow.set_min_children_per_line(3)
        presets_flow.set_row_spacing(4)
        presets_flow.set_column_spacing(4)

        presets = [
            ("5s", 0, 5),
            ("10s", 0, 10),
            ("30s", 0, 30),
            ("1 min", 1, 0),
            ("2 min", 2, 0),
            ("5 min", 5, 0),
            ("10 min", 10, 0)
        ]
        for text, m, s in presets:
            b = Gtk.Button(label=text)
            b.add_css_class("flat")
            b.connect("clicked", lambda _, min_v=m, sec_v=s: self._apply_preset(min_v, sec_v))
            presets_flow.append(b)

        inner.append(presets_flow)

        # -------------------------------------------------------------
        # Opções do Clique
        # -------------------------------------------------------------
        opt_group = Adw.PreferencesGroup(title="Opções do Botão")

        self.btn_model = Gtk.StringList.new(["Esquerdo", "Direito", "Meio"])
        self.combo_button = Adw.ComboRow(model=self.btn_model, title="Botão do Mouse")
        btn_idx = {"left": 0, "right": 1, "middle": 2}.get(self.target.button, 0)
        self.combo_button.set_selected(btn_idx)
        self.combo_button.connect("notify::selected", lambda *a: self.sync_from_ui())
        opt_group.add(self.combo_button)

        self.switch_double = Adw.SwitchRow(title="Clique Duplo")
        self.switch_double.set_active(self.target.double_click)
        self.switch_double.connect("notify::active", lambda *a: self.sync_from_ui())
        opt_group.add(self.switch_double)

        inner.append(opt_group)

        # Botão de Teste Imediato
        self.btn_test = Gtk.Button()
        self.test_content = Adw.ButtonContent(
            icon_name="input-mouse-symbolic",
            label=f"Testar clique no {self.target.name} agora"
        )
        self.btn_test.set_child(self.test_content)
        self.btn_test.connect("clicked", self._on_test_clicked)
        inner.append(self.btn_test)

    def load_from_target(self):
        """Atualiza os controles da interface a partir do objeto ClickTarget."""
        self.switch_enabled.set_active(self.target.enabled)
        self.switch_fixed.set_active(self.target.use_fixed_pos)
        if self.target.x is not None:
            self.spin_x.set_value(self.target.x)
        if self.target.y is not None:
            self.spin_y.set_value(self.target.y)
        self.spin_minutes.set_value(self.target.minutes)
        self.spin_seconds.set_value(self.target.seconds)

        btn_idx = {"left": 0, "right": 1, "middle": 2}.get(self.target.button, 0)
        self.combo_button.set_selected(btn_idx)
        self.switch_double.set_active(self.target.double_click)

        self.row_monitor_info.set_subtitle(get_monitor_info(self.target.x or 0, self.target.y or 0))
        self.update_timer_display(self.target.interval_seconds)

    def _apply_preset(self, minutes: int, seconds: int):
        self.spin_minutes.set_value(minutes)
        self.spin_seconds.set_value(seconds)
        self.sync_from_ui()

    def _on_enabled_toggled(self, switch, *args):
        self.target.enabled = switch.get_active()
        self.set_sensitive_inputs(self.target.enabled)
        self.parent_win.schedule_auto_save()

    def _on_fixed_toggled(self, switch, *args):
        is_fixed = switch.get_active()
        self.target.use_fixed_pos = is_fixed
        self.row_select.set_sensitive(is_fixed)
        self.row_monitor_info.set_sensitive(is_fixed)
        self.spin_x.set_sensitive(is_fixed)
        self.spin_y.set_sensitive(is_fixed)
        self.sync_from_ui()

    def _on_pick_clicked(self, btn):
        MultiScreenPicker(self.target.name, self._on_coords_picked)

    def _on_coords_picked(self, x: int, y: int):
        self.spin_x.set_value(x)
        self.spin_y.set_value(y)
        self.target.x = x
        self.target.y = y
        self.target.use_fixed_pos = True
        self.switch_fixed.set_active(True)
        self.row_monitor_info.set_subtitle(get_monitor_info(x, y))
        self.sync_from_ui()
        self.parent_win.show_toast(f"Posição do {self.target.name} salva: {get_monitor_info(x, y)}")

    def _on_test_clicked(self, btn):
        self.test_content.set_label("Movendo e clicando...")
        self.btn_test.set_sensitive(False)

        def do_test():
            try:
                self.sync_from_ui()
                self.engine.trigger_target_now(self.target.id)
            except Exception as e:
                print(f"[Erro no teste do {self.target.name}] {e}")
            finally:
                GLib.idle_add(self._restore_test_btn)

        import threading
        threading.Thread(target=do_test, daemon=True).start()

    def _restore_test_btn(self):
        self.test_content.set_label(f"Testar clique no {self.target.name} agora")
        self.btn_test.set_sensitive(True)
        return False

    def set_sensitive_inputs(self, active: bool):
        is_running = self.engine.is_running
        self.spin_minutes.set_sensitive(active and not is_running)
        self.spin_seconds.set_sensitive(active and not is_running)
        self.switch_fixed.set_sensitive(active and not is_running)
        self.row_select.set_sensitive(active and not is_running and self.switch_fixed.get_active())
        self.row_monitor_info.set_sensitive(active and not is_running and self.switch_fixed.get_active())
        self.spin_x.set_sensitive(active and not is_running and self.switch_fixed.get_active())
        self.spin_y.set_sensitive(active and not is_running and self.switch_fixed.get_active())
        self.combo_button.set_sensitive(active)
        self.switch_double.set_sensitive(active)
        self.btn_test.set_sensitive(active)

    def sync_from_ui(self):
        self.target.minutes = self.spin_minutes.get_value()
        self.target.seconds = self.spin_seconds.get_value()
        self.target.use_fixed_pos = self.switch_fixed.get_active()
        self.target.x = int(self.spin_x.get_value())
        self.target.y = int(self.spin_y.get_value())

        self.row_monitor_info.set_subtitle(get_monitor_info(self.target.x, self.target.y))

        btn_idx = self.combo_button.get_selected()
        btn_names = ["left", "right", "middle"]
        self.target.button = btn_names[btn_idx] if btn_idx < len(btn_names) else "left"
        self.target.double_click = self.switch_double.get_active()
        self.target.enabled = self.switch_enabled.get_active()

        if not self.engine.is_running:
            self.target.reset_timer()
            self.update_timer_display(self.target.interval_seconds)

        self.parent_win.schedule_auto_save()

    def update_timer_display(self, seconds: float):
        s = int(seconds)
        hours = s // 3600
        mins = (s % 3600) // 60
        secs = s % 60
        if hours > 0:
            formatted = f"{hours:02d}:{mins:02d}:{secs:02d}"
        else:
            formatted = f"{mins:02d}:{secs:02d}"

        self.lbl_countdown.set_label(formatted)

        if self.target.interval_seconds > 0:
            frac = min(1.0, max(0.0, seconds / self.target.interval_seconds))
            self.progress_bar.set_fraction(frac)

    def update_tick(self):
        if not self.target.enabled:
            self.lbl_status.set_label("Desativado")
            return

        if self.engine.is_running:
            self.lbl_status.set_label("Ativo - Contando")
            self.update_timer_display(self.target.remaining_seconds)
        else:
            self.lbl_status.set_label("Parado")
            self.update_timer_display(self.target.interval_seconds)

        self.lbl_clicks.set_label(f"{self.target.total_clicks} clique{'s' if self.target.total_clicks != 1 else ''}")


class AutoClickerWindow(Adw.ApplicationWindow):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.set_title("AutoClicker Multi-Ponto")
        self.set_default_size(580, 840)

        self.engine = MultiClickEngine()
        self.engine.on_tick = self._on_engine_tick
        self.engine.on_target_click = self._on_engine_target_click
        self.engine.on_status_change = self._on_engine_status_change

        self._auto_save_timeout_id = None
        self.target_widgets = []

        # Carrega configurações salvas previamente
        loaded = load_settings(self.engine)

        self._setup_ui()

        if loaded:
            for w in self.target_widgets:
                w.load_from_target()

        self.connect("close-request", self._on_window_close)

    def _setup_ui(self):
        # Toast overlay para mensagens elegantes na tela
        self.toast_overlay = Adw.ToastOverlay()
        self.set_content(self.toast_overlay)

        main_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        self.toast_overlay.set_child(main_box)

        # HeaderBar moderna
        header = Adw.HeaderBar()
        title_widget = Adw.WindowTitle(
            title="AutoClicker Multi-Ponto",
            subtitle="Suporte a múltiplos monitores (X e Y)"
        )
        header.set_title_widget(title_widget)

        # Botão Salvar Configurações
        btn_save = Gtk.Button()
        save_content = Adw.ButtonContent(
            icon_name="document-save-symbolic",
            label="Salvar"
        )
        btn_save.set_child(save_content)
        btn_save.set_tooltip_text("Salvar opções agora (~/.config/autoclicker/config.json)")
        btn_save.connect("clicked", self._on_manual_save_clicked)
        header.pack_start(btn_save)

        # Botão Sobre
        about_btn = Gtk.Button(icon_name="help-about-symbolic")
        about_btn.set_tooltip_text("Sobre o AutoClicker")
        about_btn.connect("clicked", self._show_about_dialog)
        header.pack_end(about_btn)

        main_box.append(header)

        scroll = Gtk.ScrolledWindow()
        scroll.set_vexpand(True)
        main_box.append(scroll)

        clamp = Adw.Clamp()
        clamp.set_maximum_size(620)
        clamp.set_tightening_threshold(500)
        clamp.set_margin_top(16)
        clamp.set_margin_bottom(24)
        clamp.set_margin_start(16)
        clamp.set_margin_end(16)
        scroll.set_child(clamp)

        content_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=16)
        clamp.set_child(content_box)

        # -------------------------------------------------------------
        # BOTÃO PRINCIPAL INICIAR / PARAR
        # -------------------------------------------------------------
        self.toggle_btn = Gtk.Button()
        self.toggle_btn.add_css_class("pill")
        self.toggle_btn.add_css_class("suggested-action")
        self.toggle_btn.set_size_request(-1, 58)

        self.toggle_content = Adw.ButtonContent(
            icon_name="media-playback-start-symbolic",
            label="Iniciar AutoClicker (Ambos os Pontos)"
        )
        self.toggle_btn.set_child(self.toggle_content)
        self.toggle_btn.connect("clicked", self._on_toggle_clicked)
        content_box.append(self.toggle_btn)

        # -------------------------------------------------------------
        # CARDS DOS PONTOS
        # -------------------------------------------------------------
        for target in self.engine.targets:
            card = TargetCardWidget(target, self.engine, self)
            self.target_widgets.append(card)
            content_box.append(card)

        # -------------------------------------------------------------
        # CONFIGURAÇÕES GERAIS E PERSISTÊNCIA
        # -------------------------------------------------------------
        general_group = Adw.PreferencesGroup(title="Preferências Globais")

        self.switch_sound = Adw.SwitchRow(
            title="Notificação Sonora",
            subtitle="Emite um som sutil do sistema a cada clique efetuado"
        )
        self.switch_sound.set_active(self.engine.play_sound)
        self.switch_sound.connect("notify::active", self._on_sound_toggled)
        general_group.add(self.switch_sound)

        # Linha informativa do arquivo de configurações
        row_cfg = Adw.ActionRow(
            title="Arquivo de Configurações",
            subtitle=CONFIG_FILE
        )
        btn_open_folder = Gtk.Button(icon_name="folder-open-symbolic")
        btn_open_folder.set_tooltip_text("Salvar opções agora")
        btn_open_folder.add_css_class("flat")
        btn_open_folder.connect("clicked", self._on_manual_save_clicked)
        row_cfg.add_suffix(btn_open_folder)
        general_group.add(row_cfg)

        content_box.append(general_group)

        # -------------------------------------------------------------
        # REGISTRO DE ATIVIDADES
        # -------------------------------------------------------------
        history_group = Adw.PreferencesGroup(title="Registro de Atividades")
        self.expander_history = Adw.ExpanderRow(
            title="Histórico de Cliques",
            subtitle="Nenhum clique realizado nesta sessão"
        )
        history_group.add(self.expander_history)
        content_box.append(history_group)

    def show_toast(self, message: str):
        toast = Adw.Toast.new(message)
        toast.set_timeout(3)
        self.toast_overlay.add_toast(toast)

    def schedule_auto_save(self):
        """Salva as configurações com debounce para não sobrecarregar o disco."""
        if self._auto_save_timeout_id is not None:
            GLib.source_remove(self._auto_save_timeout_id)

        def do_save():
            save_settings(self.engine)
            self._auto_save_timeout_id = None
            return False

        self._auto_save_timeout_id = GLib.timeout_add(800, do_save)

    def _on_manual_save_clicked(self, btn):
        for w in self.target_widgets:
            w.sync_from_ui()
        if save_settings(self.engine):
            self.show_toast("✓ Configurações salvas com sucesso!")
        else:
            self.show_toast("Erro ao salvar configurações.")

    def _on_sound_toggled(self, switch, *args):
        self.engine.play_sound = switch.get_active()
        self.schedule_auto_save()

    def _on_toggle_clicked(self, btn):
        if self.engine.is_running:
            self.engine.stop()
        else:
            for w in self.target_widgets:
                w.sync_from_ui()
            save_settings(self.engine)
            self.engine.start()

    def _on_engine_tick(self, targets):
        GLib.idle_add(self._ui_update_tick)

    def _ui_update_tick(self):
        for w in self.target_widgets:
            w.update_tick()
        return False

    def _on_engine_target_click(self, target: ClickTarget, timestamp: str):
        GLib.idle_add(self._ui_record_target_click, target, timestamp)

    def _ui_record_target_click(self, target: ClickTarget, timestamp: str):
        pos_str = get_monitor_info(target.x or 0, target.y or 0) if target.use_fixed_pos else "Cursor atual"
        self.expander_history.set_subtitle(f"Último clique em {target.name} às {timestamp}")

        row = Adw.ActionRow()
        row.set_title(f"{target.name} - Clique #{target.total_clicks}")
        row.set_subtitle(f"Alvo: {pos_str} | Horário: {timestamp}")
        self.expander_history.add_row(row)

        for w in self.target_widgets:
            w.update_tick()
        return False

    def _on_engine_status_change(self, is_running: bool):
        GLib.idle_add(self._ui_update_status, is_running)

    def _ui_update_status(self, is_running: bool):
        if is_running:
            self.toggle_content.set_label("Parar AutoClicker")
            self.toggle_content.set_icon_name("media-playback-stop-symbolic")
            self.toggle_btn.remove_css_class("suggested-action")
            self.toggle_btn.add_css_class("destructive-action")
        else:
            self.toggle_content.set_label("Iniciar AutoClicker (Ambos os Pontos)")
            self.toggle_content.set_icon_name("media-playback-start-symbolic")
            self.toggle_btn.remove_css_class("destructive-action")
            self.toggle_btn.add_css_class("suggested-action")

        for w in self.target_widgets:
            w.set_sensitive_inputs(w.target.enabled)
            w.update_tick()
        return False

    def _show_about_dialog(self, btn):
        dialog = Adw.AboutDialog(
            application_name="AutoClicker Multi-Ponto",
            application_icon="input-mouse-symbolic",
            developer_name="Antigravity",
            version="2.3.0",
            comments="Automação de cliques periódicos para Linux GNOME Wayland e X11.\nSuporta múltiplos monitores, salvamento automático de configurações e controle nativo via libei.",
            website="https://cachyos.org",
            license_type=Gtk.License.MIT_X11
        )
        dialog.present(self)

    def _on_window_close(self, win):
        for w in self.target_widgets:
            w.sync_from_ui()
        save_settings(self.engine)
        self.engine.close()
        return False


class AutoClickerApp(Adw.Application):
    def __init__(self):
        super().__init__(
            application_id="io.github.autoclicker",
            flags=Gio.ApplicationFlags.FLAGS_NONE
        )

    def do_activate(self):
        win = self.props.active_window
        if not win:
            win = AutoClickerWindow(application=self)
        win.present()


def run_gui():
    app = AutoClickerApp()
    return app.run(sys.argv[:1])


if __name__ == "__main__":
    run_gui()
