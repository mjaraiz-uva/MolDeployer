#include "Particle.h"

namespace Chemistry {

Reactor* Particle::s_reactor = nullptr;

void Particle::SetReactor(Reactor* reactor) {
    s_reactor = reactor;
}

Particle::Particle() : m_id(-1) {}

Particle::Particle(int id) : m_id(id) {}

Particle::~Particle() = default;

} // namespace Chemistry
