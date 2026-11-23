#include "rangierwerk/LocalRound.h"
#include <array>
#include <cassert>
#include <set>

using namespace rangierwerk;

namespace {
struct Driver {
  Yard &yard;
  Index slot;
  Consist train() const { return yard.consist(slot); }
  Index rear() const { return yard.vehicles[train().vehicles[0]].node; }
  Index position() const { return yard.vehicles[slot].node; }
  void turnout(const Siding &road, bool diverging) {
    const Index index = yard.layout.nodes[road.turnout].turnout;
    if (bool((yard.thrown >> index) & 1) != diverging)
      assert(yard.toggle(index));
  }
  void step(int direction) { assert(yard.drive(slot, direction) == Move::Travel); }
  template<class Stop>
  void run(int direction, Stop done) {
    for (int i = 0; i < MaxNodes && !done(); ++i)
      step(direction);
    assert(done());
  }
  void exit(const Siding &road) {
    run(1, [&] {
      const auto consist = train();
      for (Index i = 0; i < consist.size; ++i) {
        const auto at = yard.vehicles[consist.vehicles[i]].node;
        if (at == road.turnout || !yard.layout.isLoop(at))
          return false;
      }
      return true;
    });
    turnout(road, false);
  }
  void depart() {
    const auto &depot = yard.layout.depots[slot];
    turnout(depot, true);
    exit(depot);
  }
  void approach(const Siding &road) {
    const Index stem = yard.layout.nodes[road.turnout].links[0];
    run(1, [&] { return rear() == stem; });
    turnout(road, true);
  }
  void collect(const Siding &road, Index expected) {
    approach(road);
    for (int i = 0; i < 6 && train().size < expected + 1; ++i)
      assert(yard.drive(slot, -1) != Move::Stopped);
    assert(train().size == expected + 1);
    exit(road);
  }
  void dropAll() {
    const auto consist = train();
    assert(consist.engine > 0);
    assert(yard.split(slot, yard.vehicles[consist.vehicles[consist.engine - 1]].id, yard.vehicles[slot].id));
    assert(train().size == 1);
  }
  void park(const Siding &road) {
    approach(road);
    run(-1, [&] { return rear() == road.end; });
    assert(yard.drive(slot, -1) == Move::Stopped);
    dropAll();
    exit(road);
  }
};

void validFleet(const Yard &yard) {
  std::set<Index> positions;
  std::set<uint32_t> ids;
  for (Index i = 0; i < MaxVehicles; ++i) {
    const auto &v = yard.vehicles[i];
    if (!v.id)
      continue;
    assert(positions.insert(v.node).second);
    assert(ids.insert(v.id).second);
    for (Index next : v.links) {
      if (next == None)
        continue;
      assert(next < MaxVehicles && yard.vehicles[next].id);
      const auto &neighbor = yard.vehicles[next];
      assert(yard.layout.adjacent(v.node, neighbor.node));
      assert(neighbor.links[0] == i || neighbor.links[1] == i);
    }
  }
}

Index stored(const Yard &yard, const Siding &road) {
  Index cars = 0;
  for (Index i = Engines; i < MaxVehicles; ++i)
    if (yard.vehicles[i].id && (yard.vehicles[i].node == road.end || yard.vehicles[i].node == road.middle))
      ++cars;
  return cars;
}
Index cargoOn(const Yard &yard, Index where) {
  for (Index i = Engines; i < MaxVehicles; ++i)
    if (yard.vehicles[i].id && yard.vehicles[i].node == where)
      return yard.vehicles[i].cargo;
  return None;
}
bool ordered(const Yard &yard, Index cargo) {
  for (const auto &order : yard.orders)
    if (order.open() && order.cargo == cargo)
      return true;
  return false;
}
int orderCount(const Yard &yard) {
  int count = 0;
  for (const auto &o : yard.orders) count += o.open();
  return count;
}

void generatorsAndWitnesses() {
  std::set<uint32_t> fingerprints;
  for (uint32_t seed = 0; seed < 500; ++seed)
    for (Index level = 1; level <= 4; ++level)
      for (bool multiplayer : {false, true}) {
        Yard yard;
        assert(yard.reset(seed, level, multiplayer));
        const auto &layout = yard.layout;
        Layout again;
        assert(again.generate(seed, level, multiplayer));
        assert(again.count == layout.count && again.switchCount == layout.switchCount);
        std::set<unsigned> coordinates;
        uint32_t fingerprint = 0;
        for (Index n = 0; n < layout.count; ++n) {
          const auto &a = layout.nodes[n], &b = again.nodes[n];
          assert(a.x == b.x && a.y == b.y && a.kind == b.kind && a.cargo == b.cargo);
          assert(coordinates.insert(a.x * 256u + a.y).second);
          for (Index i = 0; i < 3; ++i)
            assert(a.links[i] == b.links[i]);
          fingerprint = fingerprint * 31u + a.x * 13u + a.y * 17u + a.links[0] * 19u + a.links[2];
        }
        fingerprints.insert(fingerprint);
        assert(layout.depotCount == (multiplayer ? 4 : 1));
        assert(layout.storageCount == 1 + level);
        assert(orderCount(yard) == (multiplayer ? 3 : level > 3 ? 3 : level));
        for (Index i = 0; i < layout.depotCount; ++i) {
          assert(yard.join(i));
          Driver driver{yard, i};
          driver.depart();
          assert(layout.isLoop(driver.position()));
          // Leave the other berths parked for the scoring route witness.
          assert(yard.recover(i));
        }
        Driver driver{yard, 0};
        driver.depart();
        for (Index road = 0; road < layout.storageCount; ++road) {
          const auto &source = layout.storage[road];
          const auto cargo = cargoOn(yard, source.end);
          if (!ordered(yard, cargo))
            continue;
          driver.collect(source, stored(yard, source));
          validFleet(yard);
          const auto before = yard.delivered;
          driver.park(layout.targets[cargo]);
          assert(yard.delivered > before);
          validFleet(yard);
          yard.tick(UnloadMs);
          break;
        }
        assert(yard.delivered > 0);
        Yard restock = yard;
        restock.tick(1000);
        assert(orderCount(restock) == orderCount(yard) || orderCount(restock) == orderCount(yard) + 1);
      }
  assert(fingerprints.size() > 1700);
  Layout fallback, second;
  assert(fallback.generate(7, 4, true, 0) && fallback.valid());
  assert(second.generate(11, 4, true, 0) && second.count == fallback.count);
  fallback.nodes[fallback.depots[0].end].links[0] = None;
  assert(!fallback.valid());
}

void movesSplitsAndHandoff() {
  Yard yard;
  assert(yard.reset(123, 4, true));
  assert(yard.join(0) && yard.join(1));
  Driver first{yard, 0}, second{yard, 1};
  const auto &depot = yard.layout.depots[0];
  assert(yard.drive(0, -1) == Move::Stopped);
  first.turnout(depot, true);
  for (int i = 0; i < 3; ++i) first.step(1);
  assert(first.position() == depot.turnout);
  assert(!yard.toggle(yard.layout.nodes[depot.turnout].turnout));
  first.step(-1);
  assert(first.position() == depot.inner);
  first.exit(depot);

  const auto &source = yard.layout.storage[0];
  first.collect(source, 2);
  const auto coupled = first.train();
  assert(coupled.engine == 2);
  const auto outer = coupled.vehicles[0], inner = coupled.vehicles[1];
  const auto outerId = yard.vehicles[outer].id, innerId = yard.vehicles[inner].id;
  auto cutAt = yard.vehicles[outer].node;
  assert(!yard.split(1, outerId, innerId));
  assert(yard.split(0, outerId, innerId));
  assert(!yard.split(0, outerId, innerId));
  assert(first.train().size == 2 && yard.vehicles[outer].node == cutAt);
  assert(yard.vehicles[outer].links[0] == None && yard.vehicles[outer].links[1] == None);
  assert(yard.recover(0)); // Keeps both parked wagons on the runaround, with their original IDs.
  second.depart();
  for (int i = 0; second.train().size < 3 && i < MaxNodes; ++i)
    assert(yard.drive(1, 1) != Move::Stopped);
  assert(second.train().size == 3 && second.train().engine == 0);
  assert(yard.vehicles[outer].id == outerId && yard.vehicles[inner].id == innerId);
  assert(yard.split(1, outerId, innerId));
  assert(second.train().size == 2);
  yard.leave(1);
  assert(yard.vehicles[inner].id == innerId && yard.vehicles[outer].id == outerId);
  validFleet(yard);
}

void trainLimitAndParkedGroup() {
  Yard yard;
  assert(yard.reset(26, 4, true) && yard.join(0) && yard.join(1));
  Driver first{yard, 0}, second{yard, 1};
  first.depart();
  first.collect(yard.layout.storage[0], 2);
  first.collect(yard.layout.storage[1], 4);
  assert(first.train().size == 5);
  const auto &road = yard.layout.storage[2];
  first.approach(road);
  first.step(-1);
  assert(!yard.toggle(yard.layout.nodes[road.turnout].turnout)); // The first wagon is on the points.
  first.step(-1);
  assert(yard.drive(0, -1) == Move::Stopped); // A fifth wagon would be coupled here.
  assert(first.train().size == 5);
  first.exit(road);
  const auto group = first.train();
  const auto firstCar = group.vehicles[0], secondCar = group.vehicles[1];
  const auto originalId = yard.vehicles[firstCar].id;
  first.dropAll();
  assert(yard.vehicles[firstCar].links[0] == secondCar || yard.vehicles[firstCar].links[1] == secondCar);
  assert(yard.recover(0));
  second.depart();
  for (int i = 0; second.train().size < 5 && i < MaxNodes; ++i)
    assert(yard.drive(1, 1) != Move::Stopped);
  assert(second.train().size == 5 && yard.vehicles[firstCar].id == originalId);
  validFleet(yard);
}

void deliveriesAndTime() {
  Yard yard;
  assert(yard.reset(17, 1, false) && yard.join(0));
  Driver driver{yard, 0};
  driver.depart();
  const auto initial = yard.orders[0];
  const auto &source = yard.layout.storage[0], &target = yard.layout.targets[initial.cargo];
  Siding wrong;
  for (Index cargo = 0; cargo < CargoTypes; ++cargo)
    if (cargo != initial.cargo && yard.layout.targets[cargo].end != None)
      wrong = yard.layout.targets[cargo];
  driver.collect(source, 1);
  driver.park(wrong);
  yard.tick(1000);
  assert(yard.delivered == 0);
  driver.collect(wrong, 1);
  driver.approach(target);
  driver.run(-1, [&] { return driver.rear() == target.end; });
  yard.tick(1000);
  assert(yard.delivered == 0); // Still coupled.
  driver.dropAll();
  assert(yard.delivered == 1);
  assert(yard.orders[0].delivered == 1);
  driver.exit(target);
  yard.tick(UnloadMs);
  assert(yard.delivered == 1 && cargoOn(yard, target.end) == None);
  if (initial.count == 2) {
    assert(yard.orders[0].id == initial.id);
    driver.collect(yard.layout.storage[1], 1);
    driver.park(target);
    yard.tick(UnloadMs);
    assert(yard.delivered == 2);
  }
  assert(yard.orders[0].id != initial.id);
  assert(orderCount(yard) == 1);

  LocalRound round;
  for (Index duration = 0; duration < 4; ++duration) {
    const uint32_t start = UINT32_MAX - 100;
    round.start(duration, start);
    const uint32_t total = LocalRound::Minutes[duration] * 60000u;
    assert(round.remaining == total);
    round.advance(yard, start + total, true);
    assert(!round.finished && round.remaining == total);
    round.advance(yard, start + total * 2 - 1, false);
    assert(!round.finished && round.remaining == 1);
    round.advance(yard, start + total * 2, false);
    assert(round.finished);
  }
  round.start(4, 0);
  round.advance(yard, 24 * 60 * 60 * 1000, false);
  assert(round.unlimited && !round.finished);
  assert(yard.reset(21, 1, false) && yard.delivered == 0);
}
} // namespace

int main() {
  generatorsAndWitnesses();
  movesSplitsAndHandoff();
  trainLimitAndParkedGroup();
  deliveriesAndTime();
}
