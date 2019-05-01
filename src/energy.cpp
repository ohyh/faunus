
#include "energy.h"

namespace Faunus {
namespace Energy {

void Energybase::to_json(json &) const {}

void Energybase::sync(Energybase *, Change &) {}

void Energybase::init() {}

void to_json(json &j, const Energybase &base) {
    assert(not base.name.empty());
    if (base.timer)
        j["relative time"] = base.timer.result();
    if (not base.cite.empty())
        j[base.name]["reference"] = base.cite;
    base.to_json(j[base.name]);
}

void EwaldData::update(const Point &box) {
    L = box;
    int kcc = std::ceil(kc);
    check_k2_zero = 0.1 * std::pow(2 * pc::pi / L.maxCoeff(), 2);
    int kVectorsLength = (2 * kcc + 1) * (2 * kcc + 1) * (2 * kcc + 1) - 1;
    if (kVectorsLength == 0) {
        kVectors.resize(3, 1);
        Aks.resize(1);
        kVectors.col(0) = Point(1, 0, 0); // Just so it is not the zero-vector
        Aks[0] = 0;
        kVectorsInUse = 1;
        Qion.resize(1);
        Qdip.resize(1);
    } else {
        double kc2 = kc * kc;
        kVectors.resize(3, kVectorsLength);
        Aks.resize(kVectorsLength);
        kVectorsInUse = 0;
        kVectors.setZero();
        Aks.setZero();
        int startValue = 1 - int(ipbc);
        for (int kx = 0; kx <= kcc; kx++) {
            double dkx2 = double(kx * kx);
            for (int ky = -kcc * startValue; ky <= kcc; ky++) {
                double dky2 = double(ky * ky);
                for (int kz = -kcc * startValue; kz <= kcc; kz++) {
                    double factor = 1.0;
                    if (kx > 0)
                        factor *= 2;
                    if (ky > 0 && ipbc)
                        factor *= 2;
                    if (kz > 0 && ipbc)
                        factor *= 2;
                    double dkz2 = double(kz * kz);
                    Point kv = 2 * pc::pi * Point(kx / L.x(), ky / L.y(), kz / L.z());
                    double k2 = kv.dot(kv);
                    if (k2 < check_k2_zero) // Check if k2 != 0
                        continue;
                    if (spherical_sum)
                        if ((dkx2 / kc2) + (dky2 / kc2) + (dkz2 / kc2) > 1)
                            continue;
                    kVectors.col(kVectorsInUse) = kv;
                    Aks[kVectorsInUse] = factor * std::exp(-k2 / (4 * alpha * alpha)) / k2;
                    kVectorsInUse++;
                }
            }
        }
        Qion.resize(kVectorsInUse);
        Qdip.resize(kVectorsInUse);
        Aks.conservativeResize(kVectorsInUse);
        kVectors.conservativeResize(3, kVectorsInUse);
    }
}

void from_json(const json &j, EwaldData &d) {
    d.alpha = j.at("alpha");
    d.rc = j.at("cutoff");
    d.kc = j.at("kcutoff");
    d.ipbc = j.value("ipbc", false);
    d.spherical_sum = j.value("spherical_sum", true);
    d.lB = pc::lB(j.at("epsr"));
    d.eps_surf = j.value("epss", 0.0);
    d.const_inf = (d.eps_surf < 1) ? 0 : 1; // if unphysical (<1) use epsr infinity for surrounding medium
}

void to_json(json &j, const EwaldData &d) {
    j = {{"lB", d.lB},
         {"ipbc", d.ipbc},
         {"epss", d.eps_surf},
         {"alpha", d.alpha},
         {"cutoff", d.rc},
         {"kcutoff", d.kc},
         {"wavefunctions", d.kVectors.cols()},
         {"spherical_sum", d.spherical_sum}};
}
std::function<double(const Particle &)> createGouyChapmanPotential(const json &j) {
    double rho;
    double c0 = j.at("ionicstrength").get<double>() * 1.0_molar; // assuming 1:1 salt, so c0=I
    double lB = pc::lB(j.at("epsr").get<double>());
    double k = 1 / (3.04 / sqrt(c0));   // hack!
    double phi0 = j.value("phi0", 0.0); // Unitless potential = beta*e*phi0
    if (std::fabs(phi0) > 1e-6)
        rho = sqrt(2 * c0 / (pc::pi * lB)) * sinh(.5 * phi0); // Evans&Wennerstrom,Colloidal Domain p
    // 138-140
    else {
        rho = 1.0 / j.value("qarea", 0.0);
        if (rho > 1e9)
            rho = j.at("rho");
        phi0 = 2. * std::asinh(rho * std::sqrt(0.5 * lB * pc::pi / c0)); //[Evans..]
    }
    double gamma0 = std::tanh(phi0 / 4); // assuming z=1  [Evans..]
    double surface_z_pos = j.value("zpos", 0.0);
    bool linearize = j.value("linearize", false);

    // return gamma function for calculation of GC potential on single particle.
    return [=](const Particle &p) {
        if (p.charge != 0) {
            double x = std::exp(-k * std::fabs(surface_z_pos - p.pos.z()));
            if (linearize)
                return p.charge * phi0 * x;
            else {
                x = gamma0 * x;
                return 2 * p.charge * std::log((1 + x) / (1 - x));
            }
        }
        return 0.0;
    };
}

double Example2D::energy(Change &) {
    double s = 1 + std::sin(2 * pc::pi * i.x()) + std::cos(2 * pc::pi * i.y());
    if (i.x() >= -2.00 && i.x() <= -1.25)
        return 1 * s;
    if (i.x() >= -1.25 && i.x() <= -0.25)
        return 2 * s;
    if (i.x() >= -0.25 && i.x() <= 0.75)
        return 3 * s;
    if (i.x() >= 0.75 && i.x() <= 1.75)
        return 4 * s;
    if (i.x() >= 1.75 && i.x() <= 2.00)
        return 5 * s;
    return 1e10;
}
double ContainerOverlap::energy(Change &change) {
    // if (spc.geo.type not_eq Geometry::CUBOID) // cuboid have PBC in all directions
    if (change) {
        // all groups have been updated
        if (change.dV or change.all) {
            for (auto &g : spc.groups) // loop over *all* groups in system
                for (auto &p : g)      // loop over *all* active particles in group
                    if (spc.geo.collision(p.pos))
                        return pc::infty;
            return 0;
        }

        // only a subset of groups have been updated
        for (auto &d : change.groups) {
            auto &g = spc.groups[d.index];
            // all atoms were updated
            if (d.all) {
                for (auto &p : g) // loop over *all* active particles in group
                    if (spc.geo.collision(p.pos))
                        return pc::infty;
            } else
                // only a subset of atoms were updated
                for (int i : d.atoms) // loop over specific atoms
                    if (spc.geo.collision((g.begin() + i)->pos))
                        return pc::infty;
        }
    }
    return 0;
}

SelfEnergy::SelfEnergy(const json &j, Tspace &spc) : spc(spc) {
    name = "selfenergy";
    type = j.at("type");
    rc = j.at("cutoff");
    epsr = j.at("epsr");
    lB = pc::lB(epsr);
    if (type == "fanourgakis")
        selfenergy_prefactor = 0.875;
    if (type == "qpotential")
        selfenergy_prefactor = 0.5;
}
double SelfEnergy::energy(Change &change) {
    double Eq = 0;
    if (change.dN)
        for (auto cg : change.groups) {
            auto g = spc.groups.at(cg.index);
            for (auto i : cg.atoms)
                if (i < g.size())
                    Eq += std::pow((g.begin() + i)->charge, 2);
        }
    else if (change.all and not change.dV)
        for (auto g : spc.groups)
            for (auto i : g)
                Eq += i.charge * i.charge;
    return -selfenergy_prefactor * Eq * lB / rc;
}
Isobaric::Isobaric(const json &j, Tspace &spc) : spc(spc) {
    name = "isobaric";
    cite = "Frenkel & Smith 2nd Ed (Eq. 5.4.13)";
    P = j.value("P/mM", 0.0) * 1.0_mM;
    if (P < 1e-10) {
        P = j.value("P/Pa", 0.0) * 1.0_Pa;
        if (P < 1e-10)
            P = j.at("P/atm").get<double>() * 1.0_atm;
    }
}
double Isobaric::energy(Change &change) {
    if (change.dV || change.all || change.dN) {
        double V = spc.geo.getVolume();
        size_t N = 0;
        for (auto &g : spc.groups)
            if (!g.empty()) {
                if (g.atomic)
                    N += g.size();
                else
                    N++;
            }
        return P * V - (N + 1) * std::log(V);
    } else
        return 0;
}
void Isobaric::to_json(json &j) const {
    j["P/atm"] = P / 1.0_atm;
    j["P/mM"] = P / 1.0_mM;
    j["P/Pa"] = P / 1.0_Pa;
    _roundjson(j, 5);
}
Constrain::Constrain(const json &j, Tspace &spc) {
    using namespace Faunus::ReactionCoordinate;
    name = "constrain";
    type = j.at("type").get<std::string>();
    try {
        if (type == "atom")
            rc = std::make_shared<AtomProperty>(j, spc);
        else if (type == "molecule")
            rc = std::make_shared<MoleculeProperty>(j, spc);
        else if (type == "system")
            rc = std::make_shared<SystemProperty>(j, spc);
        else if (type == "cmcm")
            rc = std::make_shared<MassCenterSeparation>(j, spc);
        if (rc == nullptr)
            throw std::runtime_error("unknown coordinate type");

    } catch (std::exception &e) {
        throw std::runtime_error("error for reaction coordinate '" + type + "': " + e.what() +
                                 usageTip["coords=[" + type + "]"]);
    }
}
double Constrain::energy(Change &change) {
    if (change) {
        double val = (*rc)();     // calculate reaction coordinate
        if (not rc->inRange(val)) // is it within allowed range?
            return pc::infty;     // if not, return infinite energy
    }
    return 0;
}
void Constrain::to_json(json &j) const {
    j = *rc;
    j["type"] = type;
    j.erase("resolution");
}
void Bonded::update_intra() {
    using namespace Potential;
    intra.clear();
    for (size_t i = 0; i < spc.groups.size(); i++) {
        auto &group = spc.groups.at(i);
        for (auto &bond : molecules.at(group.id).bonds) {
            intra[i].push_back(bond->clone()); // deep copy BondData from MoleculeData
            intra[i].back()->shift(std::distance(spc.p.begin(), group.begin()));
            Potential::setBondEnergyFunction(intra[i].back(), spc.p);
        }
    }
}
double Bonded::sum_energy(const Bonded::BondVector &bonds) const {
    double energy = 0;
    for (auto &bond : bonds) {
        assert(bond->hasEnergyFunction());
        energy += bond->energy(spc.geo.getDistanceFunc());
    }
    return energy;
}
double Bonded::sum_energy(const Bonded::BondVector &bonds, const std::vector<int> &particles_ndx) const {
    double energy = 0;
    // outer loop over bonds to ensure that each bond is counted at most once
    for (auto &bond : bonds) {
        for (auto particle_ndx : particles_ndx) {
            if (std::find(bond->index.begin(), bond->index.end(), particle_ndx) != bond->index.end()) {
                assert(bond->hasEnergyFunction());
                energy += bond->energy(spc.geo.getDistanceFunc());
                break; // count each interaction at most once
            }
        }
    }
    return energy;
}
Bonded::Bonded(const json &j, Tspace &spc) : spc(spc) {
    name = "bonded";
    update_intra();
    if (j.is_object())
        if (j.count("bondlist") == 1)
            inter = j["bondlist"].get<BondVector>();
    for (auto &i : inter) // set all energy functions
        Potential::setBondEnergyFunction(i, spc.p);
}
void Bonded::to_json(json &j) const {
    if (!inter.empty())
        j["bondlist"] = inter;
    if (!intra.empty()) {
        json &_j = j["bondlist-intramolecular"];
        _j = json::array();
        for (auto &i : intra)
            for (auto &b : i.second)
                _j.push_back(b);
    }
}
double Bonded::energy(Change &change) {
    double energy = 0;
    if (change) {
        energy += sum_energy(inter); // energy of inter-molecular bonds

        if (change.all || change.dV) {              // compute all active groups
            for (auto &i : intra) {                 // energies of intra-molecular bonds
                if (!spc.groups[i.first].empty()) { // add only if group is active
                    energy += sum_energy(i.second);
                }
            }
        } else { // compute only the affected groups
            for (auto &group : change.groups) {
                auto &intra_group = intra[group.index];
                if (group.internal) {
                    if (group.all) { // all internal positions updated
                        if (not spc.groups[group.index].empty())
                            energy += sum_energy(intra_group);
                    } else { // only partial update of affected atoms
                        std::vector<int> atoms_ndx;
                        // an offset is the index of the first particle in the group
                        int offset = std::distance(spc.p.begin(), spc.groups[group.index].begin());
                        // add an offset to the group atom indices to get the absolute indices
                        std::transform(group.atoms.begin(), group.atoms.end(), std::back_inserter(atoms_ndx),
                                       [offset](int i) { return i + offset; });
                        energy += sum_energy(intra_group, atoms_ndx);
                    }
                }
            }
        } // for-loop over groups
    }
    return energy;
}
} // end of namespace Energy
} // end of namespace Faunus
