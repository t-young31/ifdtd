
struct PerfectlyMatchedLayer {
  int Dxl = 0;  // Thickness of lower pml in the x direction
  int Dxu = 0;  //              upper
  int Dyl = 0;
  int Dyu = 0;
  int Dzl = 0;
  int Dzu = 0;
};

class SimulationParameters{

public:
    SimulationParameters();

    double       omega_an      = 0.0;      // Angular ω
    int          Nt            = 0;        // Number of simulation steps
    int          start_tind    = 0;        // Starting iteration number for the time steps
    double       dt            = 0.0;      // Time step
    bool         has_tdfdir    = false;    // Is the tdfdir (time domain field directory) defined?
    bool         is_multilayer = false;    // Is this simulation of a multilayer?
    bool         is_disp_ml    = false;    // Is the material dispersive?
    double       to_l          = 0.0;      // Time delay of pulse
    double       hwhm          = 0.0;      // Half width at half max of pulse
    PerfectlyMatchedLayer pml;             // Perfectly matched layer struct with size attributes
    bool         exphasorsvolume = false;   // Should phasors be extracted in the whole volume?
    bool         exphasorssurface = false;  // Should phasors be extracted on a surface?
    bool         intphasorssurface = false; // Should phasors be extracted/interpolated?
};
