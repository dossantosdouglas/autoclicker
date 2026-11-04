#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libei.h>

/*
 * ei_clicker: Utilitário nativo libei para GNOME Wayland.
 * Uso: ./ei_clicker <socket_fd> <x> <y> <button_code> <double_click (0/1)> <restore_x> <restore_y>
 * Se x < 0, apenas clica sem mover.
 * Se restore_x >= 0, restaura o cursor para (restore_x, restore_y) após o clique.
 */

int main(int argc, char **argv) {
    if (argc < 8) {
        fprintf(stderr, "Uso: %s <fd> <x> <y> <button_code> <double> <restore_x> <restore_y>\n", argv[0]);
        return 1;
    }

    int fd = atoi(argv[1]);
    double target_x = atof(argv[2]);
    double target_y = atof(argv[3]);
    int button = atoi(argv[4]);
    int is_double = atoi(argv[5]);
    double restore_x = atof(argv[6]);
    double restore_y = atof(argv[7]);

    struct ei *ei = ei_new_sender(NULL);
    ei_configure_name(ei, "autoclicker-runner");
    if (ei_setup_backend_fd(ei, fd) < 0) {
        fprintf(stderr, "Erro ao conectar socket FD ao libei\n");
        return 1;
    }

    struct ei_device *abs_dev = NULL;
    uint32_t seq = 1;

    // Dispara até encontrar o dispositivo de ponteiro absoluto e botões
    for (int i = 0; i < 40 && !abs_dev; i++) {
        ei_dispatch(ei);
        struct ei_event *e;
        while ((e = ei_get_event(ei))) {
            enum ei_event_type t = ei_event_get_type(e);
            if (t == EI_EVENT_SEAT_ADDED) {
                struct ei_seat *seat = ei_event_get_seat(e);
                ei_seat_bind_capabilities(seat,
                    EI_DEVICE_CAP_POINTER_ABSOLUTE,
                    EI_DEVICE_CAP_BUTTON,
                    NULL);
            } else if (t == EI_EVENT_DEVICE_ADDED) {
                struct ei_device *dev = ei_event_get_device(e);
                if (ei_device_has_capability(dev, EI_DEVICE_CAP_POINTER_ABSOLUTE)) {
                    abs_dev = dev;
                }
            }
            ei_event_unref(e);
        }
        usleep(15000);
    }

    if (!abs_dev) {
        fprintf(stderr, "Dispositivo virtual de ponteiro absoluto não encontrado no Mutter\n");
        ei_unref(ei);
        return 2;
    }

    usleep(30000);
    ei_dispatch(ei);

    // 1. Move o cursor até o alvo se target_x >= 0
    if (target_x >= 0 && target_y >= 0) {
        ei_device_start_emulating(abs_dev, seq++);
        ei_device_pointer_motion_absolute(abs_dev, target_x, target_y);
        ei_device_frame(abs_dev, ei_now(ei));
        ei_device_stop_emulating(abs_dev);
        ei_dispatch(ei);
        usleep(40000);
    }

    // 2. Executa o clique (simples ou duplo)
    int clicks = is_double ? 2 : 1;
    for (int c = 0; c < clicks; c++) {
        ei_device_start_emulating(abs_dev, seq++);
        ei_device_button_button(abs_dev, (uint32_t)button, 1);
        ei_device_frame(abs_dev, ei_now(ei));
        ei_dispatch(ei);

        usleep(35000);

        ei_device_button_button(abs_dev, (uint32_t)button, 0);
        ei_device_frame(abs_dev, ei_now(ei));
        ei_device_stop_emulating(abs_dev);
        ei_dispatch(ei);

        if (c < clicks - 1) {
            usleep(70000);
        }
    }

    // 3. Restaura o cursor se solicitado
    if (restore_x >= 0 && restore_y >= 0) {
        usleep(40000);
        ei_device_start_emulating(abs_dev, seq++);
        ei_device_pointer_motion_absolute(abs_dev, restore_x, restore_y);
        ei_device_frame(abs_dev, ei_now(ei));
        ei_device_stop_emulating(abs_dev);
        ei_dispatch(ei);
    }

    usleep(30000);
    ei_dispatch(ei);

    ei_unref(ei);
    return 0;
}
