/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026 Ricardo Gómez
 *
 * Aura UI -- capa de interfaz sobre este fork de Rockbox (ver
 * MODIFICATIONS.md, DECISIONS.md D-001/D-002 en la raíz del repositorio).
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
/* Test host-side del predicado puro de espera de commit (D-342). */
#include <stdio.h>
#include "../aura_switch_wait.h"

static int failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } \
} while (0)

static void test_idle_never_waits(void)
{
    /* commit_step == 0: nunca hay que esperar, sin importar los ticks. */
    CHECK(!aura_switch_wait_for_commit(0, 0, 1000));
    CHECK(!aura_switch_wait_for_commit(0, 999, 1000));
    CHECK(!aura_switch_wait_for_commit(0, 5000, 1000));
}

static void test_commit_in_progress_waits_until_deadline(void)
{
    /* commit_step != 0 y todavia no se cumple el tope: seguir esperando. */
    CHECK(aura_switch_wait_for_commit(1, 0, 1000));
    CHECK(aura_switch_wait_for_commit(3, 999, 1000));

    /* al llegar o pasar el tope, dejar de esperar aunque el commit siga. */
    CHECK(!aura_switch_wait_for_commit(1, 1000, 1000));
    CHECK(!aura_switch_wait_for_commit(1, 1001, 1000));
    CHECK(!aura_switch_wait_for_commit(7, 50000, 1000));
}

static void test_tick_wraparound(void)
{
    /* current_tick es long y da la vuelta; la comparacion con signo
     * (igual a TIME_BEFORE) debe seguir funcionando cerca del wraparound. */
    long deadline = (long)2147483000L; /* cerca del limite de long de 32 bits */
    CHECK(aura_switch_wait_for_commit(1, 2147482000L, deadline));
    CHECK(!aura_switch_wait_for_commit(1, deadline, deadline));
}

int main(void)
{
    test_idle_never_waits();
    test_commit_in_progress_waits_until_deadline();
    test_tick_wraparound();
    if (failures)
    {
        printf("test_switch_wait: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_switch_wait: OK\n");
    return 0;
}
