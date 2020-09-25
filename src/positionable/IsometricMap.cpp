#include <IsometricMap.h>
#include <IsometricServer.h>


using namespace godot;

IsometricMap::IsometricMap() : drawTiles(true), currentSortingOrder(0) {

}

void IsometricMap::_register_methods() {
    register_method("_init", &IsometricMap::_init);
    register_method("_ready", &IsometricMap::_ready);
    register_method("_process", &IsometricMap::_process);
    register_method("get_class", &IsometricMap::get_class);

    register_method("add_iso_positionable", &IsometricMap::addIsoPositionable);
    register_method("remove_iso_positionable", &IsometricMap::removeIsoPositionable);
    register_method("get_positionable_at", &IsometricMap::getPositionableAt);
    register_method("is_overlapping", &IsometricMap::isOverlapping);
    register_method("is_overlapping_aabb", &IsometricMap::isOverlappingAABB);
    register_method("are_map_elements_overlapping", &IsometricMap::areMapElementsOverlapping);
    register_method("has", &IsometricMap::has);
    register_method("get_positionable_children", &IsometricMap::getPositionableChildren);
    register_method("flatten", &IsometricMap::flatten);

    register_method("_on_resize", &IsometricMap::onResize);
    register_method("_on_grid_updated", &IsometricMap::onGridUpdated);
}

void IsometricMap::_init() {
    IsometricPositionable::_init();
}

void IsometricMap::_ready() {
    const Array &children { get_children() };
    for (int i = 0; i < children.size(); i++) {
        auto *positionable = cast_to<IsometricPositionable>(children[i]);
        if (positionable) {
            grid3D.setData(positionable->getPosition3D(), positionable);
            editionGrid3D.insertBox(positionable->getAABB(), positionable);
        }
    }
}

void IsometricMap::_process(float delta) {
    //if(Engine::get_singleton()->is_editor_hint()) to check if in editor, to deactivate code if needed
    generateTopologicalRenderGraph();
}

String IsometricMap::get_class() const {
    return "IsometricMap";
}

void IsometricMap::onResize() {
    IsometricPositionable::onResize();
    grid3D.updateArraySize(getSize3D());
    editionGrid3D.updateArraySize(getSize3D());
}

void IsometricMap::onGridUpdated(int stair) {
    const Array &children { get_children() };
    for (int i = 0; i < children.size(); i++) {
        auto *isometricPositionable = cast_to<IsometricPositionable>(children[i]);
        if (isometricPositionable) {
            isometricPositionable->onGridUpdated(stair - static_cast<int>(getPosition3D().z));
        }
    }
}

void IsometricMap::generateTopologicalRenderGraph() {
    currentSortingOrder = 0;
    const Array &children { get_children() };
    for (int i = 0; i < children.size(); i++) {
        auto *positionable = cast_to<IsometricPositionable>(children[i]);
        if (positionable) {
            positionable->setRendered(false);
        }
    }
    for (int i = 0; i < children.size(); i++) {
        auto *positionable = cast_to<IsometricPositionable>(children[i]);
        if (positionable) {
            if (!positionable->isRendered()) {
                renderIsoNode(positionable);
            }
        }
    }
}

void IsometricMap::renderIsoNode(IsometricPositionable *isoNode) {
    isoNode->setRendered(true);
    int maxZSize { 0 };
    const Array &isoNodes { getPositionableBehind(isoNode) };
    for (int i = 0; i < isoNodes.size(); i++) {
        auto *positionable = cast_to<IsometricPositionable>(isoNodes[i]);
        if (positionable) {
            if (!positionable->isRendered()) {
                renderIsoNode(positionable);
            }
            int positionableZOrderSize { positionable->getZOrderSize() };
            maxZSize = positionableZOrderSize >= maxZSize ? positionableZOrderSize : maxZSize;
        }
    }
    currentSortingOrder += maxZSize;
    isoNode->set_z_index(currentSortingOrder);
    currentSortingOrder += 1;
}

Array IsometricMap::getPositionableBehind(IsometricPositionable *isoNode) {
    Array isoNodes;
    const Array &children { get_children() };
    for (int i = 0; i < children.size(); i++) {
        auto *positionable = cast_to<IsometricPositionable>(children[i]);
        if (positionable && positionable != isoNode) {
            if (IsometricServer::doHexagoneOverlap(isoNode->getHexagoneCoordinates(), positionable->getHexagoneCoordinates())
            && IsometricServer::isBoxInFront(isoNode->getAABB(), positionable->getAABB())) {
                isoNodes.append(positionable);
            }
        }
    }
    return isoNodes;
}

Array IsometricMap::getFlattenPositionables(const Vector3 &offset) {
    Array positionables;
    const Array &children { get_children() };
    for (int i = 0; i < children.size(); i++) {
        auto *positionable = cast_to<IsometricPositionable>(children[i]);
        if (positionable) {
            if (auto *map = cast_to<IsometricMap>(positionable)) {
                const Array &innerPositionables = map->getFlattenPositionables(offset + map->getPosition3D());
                for (int j = 0; j < innerPositionables.size(); j++) {
                    auto *innerPositionable = cast_to<IsometricPositionable>(innerPositionables[j]);
                    if (innerPositionable) {
                        positionables.append(innerPositionable);
                    }
                }
            } else {
                auto *duplicatePositionable = cast_to<IsometricPositionable>(positionable->duplicate());
                duplicatePositionable->setPosition3D(offset + positionable->getPosition3D());
                positionables.append(duplicatePositionable);
            }
        }
    }
    return positionables;
}

IsometricMap *IsometricMap::initializeFrom() {
    IsometricMap *copy { IsometricMap::_new() };
    copy->setAABB(getAABB());
    return copy;
}

void IsometricMap::addIsoPositionable(IsometricPositionable *isometricPositionable) {
    const Vector3 &mapSize { getAABB().size };
    const AABB &aabb { isometricPositionable->getAABB() };
    const Vector3 &pos { aabb.position };
    bool isOverlapping;
    auto *childMap {cast_to<IsometricMap>(isometricPositionable)};
    if (childMap) {
        isOverlapping = areMapElementsOverlapping(pos, childMap);
    } else {
        isOverlapping = editionGrid3D.isOverlapping(aabb);
    }
    if (pos.x >= mapSize.x || pos.y >= mapSize.y || pos.z >= mapSize.z || isOverlapping) return;
    isometricPositionable->setTemporary(false);
    isometricPositionable->setDebugZ(0);

    grid3D.setData(aabb.position, isometricPositionable);

    if (childMap) {
        insertMapAsFlatten(childMap, pos);
    } else {
        editionGrid3D.insertBox(aabb, isometricPositionable);
    }

    add_child(isometricPositionable);
    isometricPositionable->add_to_group(ISO_GROUP, false);
}

void IsometricMap::insertMapAsFlatten(IsometricMap* map, const Vector3 &offset) {
    const Array &children { map->get_children() };
    for (int i = 0; i < children.size(); i++) {
        if (auto *positionable = cast_to<IsometricPositionable>(children[i])) {
            if (auto *m = cast_to<IsometricMap>(positionable)) {
                insertMapAsFlatten(m, offset + m->getPosition3D());
            } else {
                const AABB &aabb {positionable->getPosition3D() + offset, positionable->getSize3D()};
                editionGrid3D.insertBox(aabb, map);
            }
        }
    }
}

void IsometricMap::removeIsoPositionable(IsometricPositionable *isometricPositionable) {
    const Vector3 &mapSize { getAABB().size };
    const AABB &aabb { isometricPositionable->getAABB() };
    const Vector3 &pos { aabb.position };
    if (pos.x >= mapSize.x || pos.y >= mapSize.y || pos.z >= mapSize.z) return;
    remove_child(isometricPositionable);

    grid3D.setData(aabb.position, nullptr);

    auto *childMap {cast_to<IsometricMap>(isometricPositionable)};
    if (childMap) {
        const Array &childMapChildren {childMap->getFlattenPositionables(pos)};
        for (int i = 0; i < childMapChildren.size(); i++) {
            if (auto *posi {cast_to<IsometricPositionable>(childMapChildren[i])}) {
                editionGrid3D.insertBox(posi->getAABB(), childMap, true);
            }
        }
    } else {
        editionGrid3D.insertBox(aabb, isometricPositionable, true);
    }
    if (isometricPositionable->is_in_group(ISO_GROUP)) {
        isometricPositionable->remove_from_group(ISO_GROUP);
    }
    isometricPositionable->update();
}

IsometricPositionable *IsometricMap::getPositionableAt(Vector3 pos, bool onlyLeftUpperCorner) {
    return cast_to<IsometricPositionable>(onlyLeftUpperCorner ? grid3D.getData(pos) : editionGrid3D.getData(pos));
}

bool IsometricMap::isOverlapping(IsometricPositionable *positionable) {
    return editionGrid3D.isOverlapping(positionable->getAABB());
}

bool IsometricMap::isOverlappingAABB(AABB aabb) {
    return editionGrid3D.isOverlapping(aabb);
}

bool IsometricMap::areMapElementsOverlapping(Vector3 position, IsometricMap* map) {
    const Array& array { map->get_children() };
    for (int i = 0; i < array.size(); i++) {
        if (auto *positionable {cast_to<IsometricPositionable>(array[i])}) {
            if (auto *childMap {cast_to<IsometricMap>(positionable)}) {
                if (areMapElementsOverlapping(position + childMap->getPosition3D(), childMap)) {
                    return true;
                }
            } else {
                if (isOverlappingAABB({position + positionable->getPosition3D(), positionable->getSize3D()})) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool IsometricMap::has(IsometricPositionable *isometricPositionable) {
    return grid3D.has(isometricPositionable);
}

/**
 * Returns positionable contained in this map.
 *
 * This should not be used often.
 *
 * @return a copy array containing positionable children.
 */
Array IsometricMap::getPositionableChildren() const {
    Array positionableChildren;
    const Array &gridArray = grid3D.getInternalArray();
    for (int i = 0; i < gridArray.size(); i++) {
        const Variant &element = gridArray[i];
        if (element) {
            positionableChildren.append(element);
        }
    }
    return positionableChildren;
}

IsometricMap *IsometricMap::flatten() {
    auto *isometricMap = this->initializeFrom();
    const Array &flattenChildren { getFlattenPositionables() };
    for (int i = 0; i < flattenChildren.size(); i++) {
        auto *it = cast_to<IsometricPositionable>(flattenChildren[i]);
        if (it) {
            isometricMap->addIsoPositionable(it);
        }
    }
    return isometricMap;
}

void IsometricMap::setAABB(AABB ab) {
    IsometricPositionable::setAABB(ab);
    setHasMoved(true);
}

void IsometricMap::setHasMoved(bool hm) {
    const Array &children = get_children();
    for (int i = 0; i < children.size(); i++) {
        auto *positionable = cast_to<IsometricPositionable>(children[i]);
        if (positionable) {
            positionable->setHasMoved(true);
        }
    }
}
