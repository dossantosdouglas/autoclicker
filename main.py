#!/usr/bin/env python3
"""
Ponto de entrada principal para o AutoClicker.
Inicia a interface gráfica moderna por padrão ou a CLI caso solicitado via parâmetro (--cli).
"""

import sys
import os

# Adiciona o diretório do script ao sys.path para imports relativos
script_dir = os.path.dirname(os.path.abspath(__file__))
if script_dir not in sys.path:
    sys.path.insert(0, script_dir)


def main():
    # Se chamado com --cli ou argumentos de linha de comando diretos, executa a CLI
    cli_flags = {"--cli", "-c", "--minutes", "-m", "--seconds", "-s", "--help", "-h"}
    wants_cli = any(arg in cli_flags for arg in sys.argv[1:])

    if wants_cli:
        from cli import run_cli
        # Remove a flag --cli antes de passar para o argparse
        args = [arg for arg in sys.argv[1:] if arg not in ("--cli", "-c")]
        run_cli(args)
    else:
        # Se não há DISPLAY nem WAYLAND_DISPLAY, força modo CLI
        if not os.environ.get("DISPLAY") and not os.environ.get("WAYLAND_DISPLAY"):
            print("[Aviso] Nenhum servidor gráfico detectado. Iniciando em modo CLI...")
            from cli import run_cli
            run_cli(sys.argv[1:])
        else:
            from gui import run_gui
            sys.exit(run_gui())


if __name__ == "__main__":
    main()
