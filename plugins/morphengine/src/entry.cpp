#include <string>
extern "C" {

// Minimal entry points for your host/workspace tooling.
// Does NOT affect the JUCE plugin build.

const char* me_name()        { return "MorphEngine"; }
const char* me_description() { return "Authentic EMU Z-plane morphing filter"; }
const char* me_version()     { return "0.1.0"; }

// Optional: return path hints, run smoke tests, etc.
int me_init() { return 0; } // success

}
