/*
 * DistributedForces.cpp
 *
 *  Created on: Nov 3, 2016
 *      Author: Bernd Doser, HITS gGmbH <bernd.doser@h-its.org>
 */

#include <algorithm>
#include "CompatInteractionType.h"
#include "DistributedForces.h"
#include "gromacs/math/vec.h"
#include "gromacs/utility/fatalerror.h"
#include "Utilities.h"

namespace fda {

DistributedForces::DistributedForces(int syslen, FDASettings const& fda_settings)
 : syslen(syslen),
   indices(syslen),
   scalar_indices(syslen),
   scalar(syslen),
   summed(syslen),
   detailed(syslen),
   fda_settings(fda_settings)
{}

void DistributedForces::clear()
{
    for (auto& e : indices) e.clear();
    for (auto& e : summed) e.clear();
    for (auto& e : detailed) e.clear();
}

void DistributedForces::clear_scalar()
{
    for (auto& e : scalar_indices) e.clear();
    for (auto& e : scalar) e.clear();
}

void DistributedForces::add_summed(int i, int j, Vector const& force, InteractionType type)
{
    if (i > j) throw std::runtime_error("Only upper triangle allowed (i < j).");

    auto & summed_i = summed[i];
    auto & indices_i = indices[i];

    auto iter = std::find(indices_i.begin(), indices_i.end(), j);

    if (iter == indices_i.end()) {
        indices_i.push_back(j);
        summed_i.push_back(Force<Vector>(force, type));
    } else {
        summed_i[std::distance(indices_i.begin(), iter)] += Force<Vector>(force, type);
    }
}

void DistributedForces::add_detailed(int i, int j, Vector const& force, PureInteractionType type)
{
    if (i > j) throw std::runtime_error("Only upper triangle allowed (i < j).");

    auto & detailed_i = detailed[i];
    auto & indices_i = indices[i];

    auto iter = std::find(indices_i.begin(), indices_i.end(), j);

    if (iter == indices_i.end()) {
        indices_i.push_back(j);
        detailed_i.push_back(DetailedForce(force, type));
    } else {
        detailed_i[std::distance(indices_i.begin(), iter)].add(force, type);
    }
}

void DistributedForces::write_detailed_vector(std::ostream& os) const
{
    for (size_t i = 0; i != detailed.size(); ++i) {
        auto const& detailed_i = detailed[i];
        auto const& indices_i = indices[i];
        for (size_t p = 0; p != detailed_i.size(); ++p) {
            size_t j = indices_i[p];
            auto const& detailed_j = detailed_i[p];
            for (int type = 0; type != static_cast<int>(PureInteractionType::NUMBER); ++type) {
                if (detailed_j.number[type] == 0) continue;
                Vector const& force = detailed_j.force[type];
                os << i << " " << j << " "
                   << force[XX] << " " << force[YY] << " " << force[ZZ] << " "
                   << from_pure(static_cast<PureInteractionType>(type)) << std::endl;
            }
        }
    }
}

void DistributedForces::write_detailed_scalar(std::ostream& os, gmx::HostVector<gmx::RVec> const& x, const matrix box) const
{
    for (size_t i = 0; i != detailed.size(); ++i) {
        auto const& detailed_i = detailed[i];
        auto const& indices_i = indices[i];
        for (size_t p = 0; p != detailed_i.size(); ++p) {
            size_t j = indices_i[p];
            auto const& detailed_j = detailed_i[p];
            for (int type = 0; type != static_cast<int>(PureInteractionType::NUMBER); ++type) {
                if (detailed_j.number[type] == 0) continue;
                Vector const& force = detailed_j.force[type];
                os << i << " " << j << " "
                   << vector2signedscalar(force.get_pointer(), x[i], x[j], box, fda_settings.v2s) << " "
                   << from_pure(static_cast<PureInteractionType>(type)) << std::endl;
            }
        }
    }
}

void DistributedForces::write_summed_vector(std::ostream& os) const
{
    if (fda_settings.binary_result_file) {
        uint num = number_of_interactions(summed);
        os.write(reinterpret_cast<char*>(&num), sizeof(uint));
        for (uint i = 0; i != summed.size(); ++i) {
            if (summed[i].empty()) continue;
            os.write(reinterpret_cast<char*>(&i), sizeof(uint));
            uint num_j = summed[i].size();
            os.write(reinterpret_cast<char*>(&num_j), sizeof(uint));
            for (uint p = 0; p != num_j; ++p) {
                uint j = indices[i][p];
                os.write(reinterpret_cast<char*>(&j), sizeof(uint));
                os.write(reinterpret_cast<const char*>(summed[i][p].force.get_pointer()), 3*sizeof(real));
                os.write(reinterpret_cast<const char*>(&summed[i][p].type), sizeof(uint));
            }
        }
    } else {
        for (size_t i = 0; i != summed.size(); ++i) {
            auto const& summed_i = summed[i];
            auto const& indices_i = indices[i];
            for (size_t p = 0; p != summed_i.size(); ++p) {
                size_t j = indices_i[p];
                auto const& summed_j = summed_i[p];
                Vector const& force = summed_j.force;
                os << i << " " << j << " "
                   << force[XX] << " " << force[YY] << " " << force[ZZ] << " "
                   << summed_j.type << std::endl;
            }
        }
    }
}

void DistributedForces::write_summed_scalar(std::ostream& os, gmx::HostVector<gmx::RVec> const& x, const matrix box) const
{
    if (fda_settings.binary_result_file) {
        uint num = number_of_interactions(summed);
        os.write(reinterpret_cast<char*>(&num), sizeof(uint));
        for (uint i = 0; i != summed.size(); ++i) {
            if (summed[i].empty()) continue;
            os.write(reinterpret_cast<char*>(&i), sizeof(uint));
            uint num_j = summed[i].size();
            os.write(reinterpret_cast<char*>(&num_j), sizeof(uint));
            for (uint p = 0; p != num_j; ++p) {
                uint j = indices[i][p];
                os.write(reinterpret_cast<char*>(&j), sizeof(uint));
                real scalar = vector2signedscalar(summed[i][p].force.get_pointer(), x[i], x[j], box, fda_settings.v2s);
                os.write(reinterpret_cast<const char*>(&scalar), sizeof(real));
                os.write(reinterpret_cast<const char*>(&summed[i][p].type), sizeof(uint));
            }
        }
    } else {
        for (size_t i = 0; i != summed.size(); ++i) {
            auto const& summed_i = summed[i];
            auto const& indices_i = indices[i];
            for (size_t p = 0; p != summed_i.size(); ++p) {
                size_t j = indices_i[p];
                auto const& summed_j = summed_i[p];
                os << i << " " << j << " "
                   << vector2signedscalar(summed_j.force.get_pointer(), x[i], x[j], box, fda_settings.v2s) << " "
                   << summed_j.type << std::endl;
            }
        }
    }
}

void DistributedForces::write_scalar(std::ostream& os) const
{
    if (fda_settings.binary_result_file) {
        uint num = number_of_interactions(scalar);
        os.write(reinterpret_cast<char*>(&num), sizeof(uint));
        for (uint i = 0; i != scalar.size(); ++i) {
            if (scalar[i].empty()) continue;
            os.write(reinterpret_cast<char*>(&i), sizeof(uint));
            uint num_j = scalar[i].size();
            os.write(reinterpret_cast<char*>(&num_j), sizeof(uint));
            for (uint p = 0; p != num_j; ++p) {
                uint j = indices[i][p];
                os.write(reinterpret_cast<char*>(&j), sizeof(uint));
                os.write(reinterpret_cast<const char*>(&scalar[i][p].force), sizeof(real));
                os.write(reinterpret_cast<const char*>(&scalar[i][p].type), sizeof(uint));
            }
        }
    } else {
        for (size_t i = 0; i != scalar.size(); ++i) {
            auto const& scalar_i = scalar[i];
            auto const& scalar_indices_i = scalar_indices[i];
            for (size_t p = 0; p != scalar_i.size(); ++p) {
                size_t j = scalar_indices_i[p];
                auto const& scalar_j = scalar_i[p];
                os << i << " " << j << " "
                   << scalar_j.force << " "
                   << scalar_j.type << std::endl;
            }
        }
    }
}

void DistributedForces::write_total_forces(std::ostream& os, gmx::HostVector<gmx::RVec> const& x, bool normalize_psr) const
{
    std::vector<real> total_forces(syslen, 0.0);
    for (size_t i = 0; i != summed.size(); ++i) {
        auto const& summed_i = summed[i];
        auto const& indices_i = indices[i];
        for (size_t p = 0; p != summed_i.size(); ++p) {
            size_t j = indices_i[p];
            auto const& summed_j = summed_i[p];
            real scalar_force;
            switch (fda_settings.v2s) {
                case Vector2Scalar::NORM:
                    scalar_force = norm(summed_j.force.get_pointer());
                    break;
                case Vector2Scalar::PROJECTION:
                    scalar_force = vector2unsignedscalar(summed_j.force.get_pointer(), i, j, x);
                    break;
                default:
                    gmx_fatal(FARGS, "Unknown option for Vector2Scalar.\n");
                    break;
            }
            total_forces[i] += scalar_force;
            total_forces[j] += scalar_force;
        }
    }

    // Detect the last non-zero item
    // nb_non_zero_forces holds the index of first zero item or the length of force
    uint nb_non_zero_forces = total_forces.size();
    if (fda_settings.no_end_zeros) {
        for (; nb_non_zero_forces > 0; --nb_non_zero_forces)
            if (total_forces[nb_non_zero_forces - 1] != 0.0)
                break;
    }

    if (normalize_psr) {
        for (uint i = 0; i < nb_non_zero_forces; ++i) {
            if (std::abs(total_forces[i]) != 0.0) total_forces[i] /= fda_settings.residue_size[i];
        }
    }

    if (fda_settings.binary_result_file) {
    	static bool was_called = false;
        if (was_called) {
        	os.write(reinterpret_cast<char*>(&nb_non_zero_forces), sizeof(uint));
        	was_called = true;
        }
        os.write(reinterpret_cast<char*>(&total_forces[0]), nb_non_zero_forces * sizeof(real));
    } else {
        bool first_on_line = true;
        for (uint i = 0; i < nb_non_zero_forces; ++i) {
            if (first_on_line) {
                os << total_forces[i];
                first_on_line = false;
            } else {
                os << " " << total_forces[i];
            }
        }
        os << std::endl;
    }
}

void DistributedForces::write_scalar_compat_ascii(std::ostream& os) const
{
    // Print total number of interactions
    int nb_interactions = 0;
    for (auto const& s : scalar) nb_interactions += s.size();
    os << nb_interactions << std::endl;

    // Print atom indices which have interactions
    for (size_t i = 0; i != scalar.size(); ++i) {
        if (!scalar[i].empty()) os << i << " ";
    }
    os << std::endl;

    // Print indices
    for (size_t i = 0; i != scalar.size(); ++i) {
        auto const& scalar_i = scalar[i];
        auto const& scalar_indices_i = scalar_indices[i];
        for (size_t p = 0; p != scalar_i.size(); ++p) {
            size_t j = scalar_indices_i[p];
            os << (i > j ? j * syslen + i : i * syslen + j) << " ";
        }
    }
    os << std::endl;

    // Print forces
    for (size_t i = 0; i != scalar.size(); ++i) {
        auto const& scalar_i = scalar[i];
        for (size_t p = 0; p != scalar_i.size(); ++p) {
            auto const& scalar_j = scalar_i[p];
            os << scalar_j.force << " ";
        }
    }
    os << std::endl;

    // Print types
    for (size_t i = 0; i != scalar.size(); ++i) {
        auto const& scalar_i = scalar[i];
        for (size_t p = 0; p != scalar_i.size(); ++p) {
            auto const& scalar_j = scalar_i[p];
            os << to_index(to_compat(scalar_j.type)) << " ";
        }
    }
    os << std::endl;
}

void DistributedForces::write_summed_compat_ascii(std::ostream& os, gmx::HostVector<gmx::RVec> const& x, const matrix box) const
{
    // Print total number of interactions
    int nb_interactions = 0;
    for (auto const& s : summed) nb_interactions += s.size();
    os << nb_interactions << std::endl;

    // Print atom indices which have interactions
    for (size_t i = 0; i != summed.size(); ++i) {
        if (!summed[i].empty()) os << i << " ";
    }
    os << std::endl;

    // Print indices
    for (size_t i = 0; i != summed.size(); ++i) {
        auto const& summed_i = summed[i];
        auto const& summed_indices_i = indices[i];
        for (size_t p = 0; p != summed_i.size(); ++p) {
            size_t j = summed_indices_i[p];
            os << (i > j ? j * syslen + i : i * syslen + j) << " ";
        }
    }
    os << std::endl;

    // Print forces
    for (size_t i = 0; i != summed.size(); ++i) {
        auto const& summed_i = summed[i];
        auto const& summed_indices_i = indices[i];
        for (size_t p = 0; p != summed_i.size(); ++p) {
            size_t j = summed_indices_i[p];
            auto const& summed_j = summed_i[p];
            os << vector2signedscalar(summed_j.force.get_pointer(), x[i], x[j], box, fda_settings.v2s) << " ";
        }
    }
    os << std::endl;

    // Print types
    for (size_t i = 0; i != summed.size(); ++i) {
        auto const& summed_i = summed[i];
        for (size_t p = 0; p != summed_i.size(); ++p) {
            auto const& summed_j = summed_i[p];
            os << to_index(to_compat(summed_j.type)) << " ";
        }
    }
    os << std::endl;
}

void DistributedForces::write_scalar_compat_bin(std::ostream& os) const
{
    // Print total number of interactions
    int nb_interactions = 0;
    for (auto const& s : scalar) nb_interactions += s.size();
    os.write(reinterpret_cast<const char*>(&nb_interactions), sizeof(nb_interactions));

    // Print atom indices which have interactions
    for (size_t i = 0; i != scalar.size(); ++i) {
        if (!scalar[i].empty())
            os.write(reinterpret_cast<const char*>(&i), sizeof(i));
    }

    // Print indices
    for (size_t i = 0; i != scalar.size(); ++i) {
        auto const& scalar_i = scalar[i];
        auto const& scalar_indices_i = scalar_indices[i];
        for (size_t p = 0; p != scalar_i.size(); ++p) {
            size_t j = scalar_indices_i[p];
            int index = i > j ? j * syslen + i : i * syslen + j;
            os.write(reinterpret_cast<const char*>(&index), sizeof(index));
        }
    }

    // Print forces
    for (size_t i = 0; i != scalar.size(); ++i) {
        auto const& scalar_i = scalar[i];
        for (size_t p = 0; p != scalar_i.size(); ++p) {
            auto const& scalar_j = scalar_i[p];
            os.write(reinterpret_cast<const char*>(&scalar_j.force), sizeof(scalar_j.force));
        }
    }

    // Print types
    for (size_t i = 0; i != scalar.size(); ++i) {
        auto const& scalar_i = scalar[i];
        for (size_t p = 0; p != scalar_i.size(); ++p) {
            auto const& scalar_j = scalar_i[p];
            auto type = to_index(to_compat(scalar_j.type));
            os.write(reinterpret_cast<const char*>(&type), sizeof(type));
        }
    }
}

void DistributedForces::write_summed_compat_bin(std::ostream& os, gmx::HostVector<gmx::RVec> const& x, const matrix box) const
{
    // Print total number of interactions
    int nb_interactions = 0;
    for (auto const& s : summed) nb_interactions += s.size();
    os.write(reinterpret_cast<const char*>(&nb_interactions), sizeof(nb_interactions));

    // Print atom indices which have interactions
    for (size_t i = 0; i != summed.size(); ++i) {
        if (!scalar[i].empty())
            os.write(reinterpret_cast<const char*>(&i), sizeof(i));
    }

    // Print indices
    for (size_t i = 0; i != summed.size(); ++i) {
        auto const& summed_i = summed[i];
        auto const& summed_indices_i = indices[i];
        for (size_t p = 0; p != summed_i.size(); ++p) {
            size_t j = summed_indices_i[p];
            int index = i > j ? j * syslen + i : i * syslen + j;
            os.write(reinterpret_cast<const char*>(&index), sizeof(index));
        }
    }

    // Print forces
    for (size_t i = 0; i != summed.size(); ++i) {
        auto const& summed_i = summed[i];
        auto const& summed_indices_i = indices[i];
        for (size_t p = 0; p != summed_i.size(); ++p) {
            size_t j = summed_indices_i[p];
            auto const& summed_j = summed_i[p];
            real force = vector2signedscalar(summed_j.force.get_pointer(), x[i], x[j], box, fda_settings.v2s);
            os.write(reinterpret_cast<const char*>(&force), sizeof(force));
        }
    }

    // Print types
    for (size_t i = 0; i != summed.size(); ++i) {
        auto const& summed_i = summed[i];
        for (size_t p = 0; p != summed_i.size(); ++p) {
            auto const& summed_j = summed_i[p];
            auto type = to_index(to_compat(summed_j.type));
            os.write(reinterpret_cast<const char*>(&type), sizeof(type));
        }
    }
}

void DistributedForces::scalar_real_divide(real divisor)
{
    real inv = 1.0 / divisor;
    for (auto& scalar_i : scalar)
        for (auto& scalar_j : scalar_i) scalar_j.force *= inv;
}

void DistributedForces::summed_merge_to_scalar(gmx::HostVector<gmx::RVec> const& x, const matrix box)
{
    for (size_t i = 0; i != summed.size(); ++i) {
        auto & scalar_i = scalar[i];
        auto & scalar_indices_i = scalar_indices[i];
        auto const& summed_i = summed[i];
        auto const& indices_i = indices[i];
        for (size_t p = 0; p != summed_i.size(); ++p) {
            size_t j = indices_i[p];
            auto const& summed_j = summed_i[p];
            auto iter = std::find(scalar_indices_i.begin(), scalar_indices_i.end(), j);
            Force<real> scalar_force(vector2signedscalar(summed_j.force.get_pointer(), x[i], x[j], box, fda_settings.v2s), summed_j.type);
            if (iter == scalar_indices_i.end()) {
                scalar_indices_i.push_back(j);
                scalar_i.push_back(scalar_force);
            } else {
                scalar_i[std::distance(scalar_indices_i.begin(), iter)] += scalar_force;
            }
        }
    }
}

template <class T>
int DistributedForces::number_of_interactions(std::vector<T> const& v) const
{
    int number_of_interactions = 0;
    for (auto&& e : v) number_of_interactions += e.size();
    return number_of_interactions;
}

} // namespace fda
