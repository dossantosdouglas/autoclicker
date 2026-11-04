"""
Interface de Linha de Comando (CLI) para o AutoClicker Multi-Ponto.
"""

import sys
import time
import argparse
import signal
from backend import MultiClickEngine, ClickTarget


def run_cli(args=None):
    parser = argparse.ArgumentParser(
        description="AutoClicker CLI Multi-Ponto - Cliques periódicos em 2 locais com tempos independentes."
    )
    # Ponto 1 (Tempo X)
    parser.add_argument(
        "-m1", "--minutes1", "-m", "--minutes",
        type=float,
        default=2.0,
        help="Tempo X: Minutos entre cliques no Ponto 1. Padrão: 2.0"
    )
    parser.add_argument(
        "-s1", "--seconds1", "-s", "--seconds",
        type=float,
        default=0.0,
        help="Segundos adicionais para o Ponto 1. Padrão: 0"
    )
    parser.add_argument(
        "-x1", type=int, default=None,
        help="Coordenada X na tela para o Ponto 1 (omitir para usar cursor atual)"
    )
    parser.add_argument(
        "-y1", type=int, default=None,
        help="Coordenada Y na tela para o Ponto 1 (omitir para usar cursor atual)"
    )
    parser.add_argument(
        "-b1", "--button1", choices=["left", "right", "middle"], default="left",
        help="Botão do mouse para o Ponto 1. Padrão: left"
    )

    # Ponto 2 (Tempo Y)
    parser.add_argument(
        "-m2", "--minutes2",
        type=float,
        default=5.0,
        help="Tempo Y: Minutos entre cliques no Ponto 2. Padrão: 5.0"
    )
    parser.add_argument(
        "-s2", "--seconds2",
        type=float,
        default=0.0,
        help="Segundos adicionais para o Ponto 2. Padrão: 0"
    )
    parser.add_argument(
        "-x2", type=int, default=None,
        help="Coordenada X na tela para o Ponto 2 (omitir para usar cursor atual)"
    )
    parser.add_argument(
        "-y2", type=int, default=None,
        help="Coordenada Y na tela para o Ponto 2 (omitir para usar cursor atual)"
    )
    parser.add_argument(
        "-b2", "--button2", choices=["left", "right", "middle"], default="left",
        help="Botão do mouse para o Ponto 2. Padrão: left"
    )
    parser.add_argument(
        "--disable-point2", action="store_true",
        help="Desativa o Ponto 2, executando apenas o Ponto 1"
    )

    # Opções Globais
    parser.add_argument(
        "--no-sound", action="store_true",
        help="Desativar sons de notificação de clique"
    )

    parsed = parser.parse_args(args)

    engine = MultiClickEngine()
    engine.play_sound = not parsed.no_sound

    # Configura Ponto 1
    t1 = engine.targets[0]
    t1.minutes = parsed.minutes1
    t1.seconds = parsed.seconds1
    t1.x = parsed.x1
    t1.y = parsed.y1
    t1.use_fixed_pos = (parsed.x1 is not None and parsed.y1 is not None)
    t1.button = parsed.button1
    t1.enabled = True

    # Configura Ponto 2
    t2 = engine.targets[1]
    t2.minutes = parsed.minutes2
    t2.seconds = parsed.seconds2
    t2.x = parsed.x2
    t2.y = parsed.y2
    t2.use_fixed_pos = (parsed.x2 is not None and parsed.y2 is not None)
    t2.button = parsed.button2
    t2.enabled = not parsed.disable_point2

    print("=" * 65)
    print("        🖱️  AUTOCLICKER MULTI-PONTO EM EXECUÇÃO (CLI)")
    print("=" * 65)
    p1_pos = f"({t1.x}, {t1.y})" if t1.use_fixed_pos else "Cursor atual"
    print(f" • Ponto 1: Intervalo = {t1.minutes}m {t1.seconds}s | Local = {p1_pos} | Botão = {t1.button}")
    if t2.enabled:
        p2_pos = f"({t2.x}, {t2.y})" if t2.use_fixed_pos else "Cursor atual"
        print(f" • Ponto 2: Intervalo = {t2.minutes}m {t2.seconds}s | Local = {p2_pos} | Botão = {t2.button}")
    else:
        print(" • Ponto 2: Desativado")
    print(f" • Som: {'Sim' if engine.play_sound else 'Não'}")
    print(" • Para parar: pressione Ctrl + C")
    print("=" * 65)
    print()

    def on_click(target, timestamp):
        pos_str = f"({target.x}, {target.y})" if target.use_fixed_pos else "Cursor atual"
        sys.stdout.write(f"\n[{target.name}] Clique #{target.total_clicks} em {pos_str} às {timestamp}!\n")
        sys.stdout.flush()

    def on_tick(targets):
        parts = []
        for t in targets:
            if t.enabled:
                mins = int(t.remaining_seconds) // 60
                secs = int(t.remaining_seconds) % 60
                parts.append(f"{t.name}: {mins:02d}:{secs:02d} ({t.total_clicks}x)")
        line = " | ".join(parts)
        sys.stdout.write(f"\r⏳ {line} ")
        sys.stdout.flush()

    engine.on_target_click = on_click
    engine.on_tick = on_tick

    def handle_sigint(sig, frame):
        print("\n\nEncerrando AutoClicker...")
        engine.stop()
        engine.close()
        print("Finalizado com sucesso.")
        sys.exit(0)

    signal.signal(signal.SIGINT, handle_sigint)
    engine.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        handle_sigint(None, None)


if __name__ == "__main__":
    run_cli()
