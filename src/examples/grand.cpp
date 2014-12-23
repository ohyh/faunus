#include <faunus/faunus.h>
#include <faunus/ewald.h>
using namespace Faunus;
using namespace Faunus::Potential;

typedef Space<Geometry::Sphere,PointParticle> Tspace;

int main() {
  InputMap mcp("grand.input");                  // open user input file
  Tspace spc(mcp);                              // simulation space
  Energy::Nonbonded<Tspace,CoulombHS> pot(mcp); // hamiltonian
  pot.setSpace(spc);                            // share space w. hamiltonian

  Group salt;                                   // group for salt particles
  salt.addParticles(spc, mcp);
  spc.load("state",Tspace::RESIZE);             // load old config. from disk (if any)

  // Two different Widom analysis methods
  double lB = Coulomb(mcp).bjerrumLength();     // get bjerrum length
  Analysis::Widom<PointParticle> widom1;        // widom analysis (I)
  Analysis::WidomScaled<Tspace> widom2(lB,1);   // ...and (II)
  widom1.add(spc.p);
  widom2.add(spc.p);

  Move::GrandCanonicalSalt<Tspace> gc(mcp,pot,spc,salt);
  Move::AtomicTranslation<Tspace> mv(mcp,pot,spc);
  mv.setGroup(salt);

  EnergyDrift sys;                              // class for tracking system energy drifts
  sys.init(Energy::systemEnergy(spc,pot,spc.p));// store initial total system energy

  cout << atom.info() + spc.info() + pot.info() + "\n";

  MCLoop loop(mcp);                             // class for handling mc loops
  while ( loop[0] ) {
    while ( loop[1] ) {
      if (slump() < 0.5)
        sys+=mv.move( salt.size() );            // translate salt
      else 
        sys+=gc.move();                         // grand canonical exchange
      widom1.sample(spc,pot,1);
      widom2.sample(spc.p,spc.geo);
    }                                           // end of micro loop
    sys.checkDrift(Energy::systemEnergy(spc,pot,spc.p)); // calc. energy drift
    cout << loop.timing();
  }                                             // end of macro loop

  FormatPQR::save("confout.pqr", spc.p);        // PQR snapshot for VMD etc.
  spc.save("state");                            // final simulation state

  UnitTest test(mcp);                           // class for unit testing
  gc.test(test);
  mv.test(test);
  sys.test(test);
  widom1.test(test);

  cout << loop.info() + sys.info() + mv.info() + gc.info() + test.info()
    + widom1.info() + widom2.info();

  return test.numFailed();
}

/**
  @page example_grand Example: Grand Canonical Salt

  This is an example of a grand canonical salt solution (NmuT)
  with the following MC moves:

  - salt translation
  - salt exchange with virtual bulk - i.e. constant chemical potential

  In addition, two different Widom methods are used to check the chemical
  potential. Note that this analysis is formally valid only in the canonical
  ensemble which may lead to deviations at small system sizes.
  It is, however, easy to turn off GC moves via the input file.

  Run this example from the terminal:

      $ cd src/examples
      $ python grand.py
  
  Input
  =====

  In this example a python script is used to generate the following two input files
  as well as run the executable.

  grand.input
  -----------

      temperature               300                  # (kelvin)
      epsilon_r                 80                   # dielectric constant
      sphere_radius             80                   # radius of container (angstrom)
      atomlist                  grand.json           # atom property file
      loop_macrosteps           10                   # number of macro loops
      loop_microsteps           20000                # number of micro loops
      tion1                     Na                   # ion type 1
      tion2                     Cl                   # ion type 2
      nion1                     20                   # initial number of ion type 1
      nion2                     20                   # initial number of ion type 2
      saltbath_runfraction      1.0                  # chance of running GC move (1=100%)
      mv_particle_runfraction   0.01                 # chance of translating particles (1=100%)
      test_stable               no                   # create (yes) or check test file (no)
      test_file                 grand.test           # name of test file to create or load

  grand.json
  ----------

  Atom properties are described in a JSON file -- for more details of
  possible properties, see `Faunus::AtomMap`. Note that standard python
  supports reading and writing of the JSON file format.

      {
        "atomlist": {
          "Na": {
            "q": 1.0, 
            "r": 2.0, 
            "dp": 50, 
            "activity": 0.05
          }, 
          "Cl": {
            "q": -1.0, 
            "r": 2.0, 
            "dp": 50, 
            "activity": 0.05
          }
        }
      }

  Output
  ======
  This is the output generated by `grand.cpp`. Note that the default is to
  use Unicode UTF-16 encoding to print mathematical symbols. If your terminal is unable to
  print this properly, Unicode output can be switched off during compilation.

  @include examples/grand.out

  grand.cpp
  =========
  @includelineno examples/grand.cpp

 
*/
