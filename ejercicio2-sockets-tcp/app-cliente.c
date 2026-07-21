#include "claves.h"
#include <stdio.h>
#include <string.h>

int main() {
  printf("\nINICIANDO BATERIA DE PRUEBAS\n");

  // preparamos datos de prueba
  float v2[3] = {1.1, 2.2, 3.3};
  struct Paquete p3 = {10, 20, 30};
  int res;

  // destrucción inicial por si había basura
  destroy();
  printf("[1] destroy(): OK Base de datos limpia\n");

  printf("\nPRUEBAS DE EXITO\n");

  // insercion
  res = set_value("clave_exito", "Valor de prueba", 3, v2, p3);
  if (res == 0)
    printf("[2] set_value(): OK clave insertada\n");
  else if (res == -2) {
    printf("ERROR: Servidor desconectado\n");
    return -1;
  }

  // existencia
  if (exist("clave_exito") == 1)
    printf("[3] exist(): OK la clave existe\n");

  // recuperacion
  char val1[256];
  int n2;
  float v2_out[32];
  struct Paquete p3_out;
  if (get_value("clave_exito", val1, &n2, v2_out, &p3_out) == 0) {
    printf("[4] get_value(): OK leido: %s, N:%d, p.x:%d)\n", val1, n2,
           p3_out.x);
  }

  // modificacion
  float v2_mod[3] = {9.9, 8.8, 7.7};
  struct Paquete p3_mod = {99, 88, 77};
  if (modify_value("clave_exito", "Valor Modificado", 3, v2_mod, p3_mod) == 0) {
    printf("[5] modify_value(): OK valores actualizados\n");
  }

  // borrado
  if (delete_key("clave_exito") == 0) {
    printf("[6] delete_key(): OK Clave borrada\n");
  }

  res = set_value("clave_invalida", "Valor", 40, v2, p3);
  if (res == -1)
    printf("[7] set_value() con N=40: OK rechazado localmente \n");

  printf("\nPRUEBAS DE CONSISTENCIA\n");
  set_value("clave_unica", "Primer valor", 3, v2, p3); // insertamos una vez
  res = set_value("clave_unica", "Intento duplicado", 3, v2,
                  p3); // intentamos duplicar
  if (res == -1)
    printf("[8] set_value() duplicado: OK rechazado por el servidor \n");

  printf("\nFIN DE LAS PRUEBAS\n\n");
  return 0;
}