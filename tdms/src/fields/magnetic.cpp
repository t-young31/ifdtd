#include "globals.h"
#include "field.h"

using namespace std;


double MagneticField::phase(int n, double omega, double dt){
  return omega * ((double) n + 0.5) * dt;
}
