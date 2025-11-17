// LuaComponentBindings.inc
// Only BEGIN_COMPONENT / PROPERTY / END_COMPONENT lines in this file.
// Edit this file to add/remove properties. Nothing else.

// Value-like types
BEGIN_COMPONENT(Vector3D, "Vector3D")
    PROPERTY("x", &Vector3D::x)
    PROPERTY("y", &Vector3D::y)
    PROPERTY("z", &Vector3D::z)
END_COMPONENT()

BEGIN_COMPONENT(Quaternion, "Quaternion")
    PROPERTY("w", &Quaternion::w)
    PROPERTY("x", &Quaternion::x)
    PROPERTY("y", &Quaternion::y)
    PROPERTY("z", &Quaternion::z)
END_COMPONENT()

// Entity-component example
BEGIN_COMPONENT(Transform, "Transform")
    PROPERTY("overrideFromPrefab", &Transform::overrideFromPrefab)
    PROPERTY("localPosition", &Transform::localPosition)
    PROPERTY("localScale", &Transform::localScale)
    PROPERTY("localRotation", &Transform::localRotation)
    PROPERTY("isDirty", &Transform::isDirty)
END_COMPONENT()

// Add more BEGIN_COMPONENT(...) ... PROPERTY(...) ... END_COMPONENT() blocks here.
