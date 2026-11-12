# Generated replacements, never modifications of the verified dependency cache.
option(RS3D_CONTACT_OBSERVER "Build locked Jolt plus read-only contact observer" ON)
set(RS3D_OBSERVER_VERSION "solver-observer-v1")
set(RS3D_OBSERVER_HASH "disabled")
set(RS3D_OBSERVER_GENERATED_HASH "none")
if(RS3D_CONTACT_OBSERVER)
  set(observer_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/observer")
  file(MAKE_DIRECTORY "${observer_dir}")
  function(observer_replace variable before after)
    string(FIND "${${variable}}" "${before}" found)
    if(found LESS 0)
      message(FATAL_ERROR "Observer patch anchor missing: ${before}")
    endif()
    string(REPLACE "${before}" "${after}" result "${${variable}}")
    set(${variable} "${result}" PARENT_SCOPE)
  endfunction()
  file(READ "${CMAKE_CURRENT_LIST_FILE}" patch)
  file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/observer/jolt_observer.h" bridge_hash)
  string(SHA256 RS3D_OBSERVER_HASH "${patch}${bridge_hash}")
  set(audit "observer_version=${RS3D_OBSERVER_VERSION}\npatch_sha256=${RS3D_OBSERVER_HASH}\nupstream_sha=0373ec0dd762e4bc2f6acdb08371ee84fa23c6db\n")
  foreach(part IN ITEMS ContactConstraintManager PhysicsSystem)
    if(part STREQUAL "ContactConstraintManager")
      set(relative "Jolt/Physics/Constraints/${part}.cpp")
      set(expected "2d2085f029d3bb52c595c8dfebbb3eea827f427f0c7868567285aa6b93a450ed")
    else()
      set(relative "Jolt/Physics/${part}.cpp")
      set(expected "7c98e24e4457cb27270a839b815078768df489f2a5e4a2baaaf0037d0fe8a1e9")
    endif()
    set(original "${RS_JoltPhysics_SOURCE_DIR}/${relative}")
    file(SHA256 "${original}" input_hash)
    if(NOT input_hash STREQUAL expected)
      message(FATAL_ERROR "Observer upstream bytes mismatch: ${relative}: ${input_hash}")
    endif()
    file(READ "${original}" code)
    observer_replace(code "#include <Jolt/Jolt.h>" "#include <Jolt/Jolt.h>\n#include \"jolt_observer.h\"")
    if(part STREQUAL "ContactConstraintManager")
      observer_replace(code "constraint.mSortKey = input_hash;" "constraint.mSortKey = input_hash;\n\t\t\tif (rs3d_observer::sink) rs3d_observer::sink->bind(&constraint, input_key);")
      observer_replace(code "constraint.mSortKey = key_hash;" "constraint.mSortKey = key_hash;\n\t\tif (rs3d_observer::sink) rs3d_observer::sink->bind(&constraint, key);")
      observer_replace(code "wcp.mContactPoint->mNonPenetrationLambda = wcp.mNonPenetrationConstraint.GetTotalLambda();" [=[wcp.mContactPoint->mNonPenetrationLambda = wcp.mNonPenetrationConstraint.GetTotalLambda();
            if (rs3d_observer::sink) rs3d_observer::sink->solved(&constraint, *constraint.mBody1, *constraint.mBody2,
                constraint.mBody1->GetCenterOfMassTransform() * Vec3::sLoadFloat3Unsafe(wcp.mContactPoint->mPosition1),
                constraint.mBody2->GetCenterOfMassTransform() * Vec3::sLoadFloat3Unsafe(wcp.mContactPoint->mPosition2),
                constraint.GetWorldSpaceNormal(), wcp.mContactPoint->mNonPenetrationLambda);]=])
    else()
      observer_replace(code "// Call contact point callbacks\n\t\t\tmContactManager.OnCCDContactAdded" [=[// Copy the exact query manifold and translated query poses, before response.
            if (rs3d_observer::sink) rs3d_observer::sink->candidate(body, body2, manifold,
                body.GetCenterOfMassPosition() + ccd_body.mFraction * ccd_body.mDeltaPosition,
                body2.GetCenterOfMassPosition() + ccd_body.mFraction * sCalculateBodyMotion(body2, ioContext->mStepDeltaTime), ccd_body.mFraction);
            // Call contact point callbacks
			mContactManager.OnCCDContactAdded]=])
      observer_replace(code "ccd_body2->mBodyID2 = BodyID();" [=[if (rs3d_observer::sink) rs3d_observer::sink->discarded(ccd_body2->mBodyID1, ccd_body2->mBodyID2);
                        ccd_body2->mBodyID2 = BodyID();]=])
      observer_replace(code "contact_constraint.SolveVelocityConstraintWithMassOverride(body1, inv_m1, body2, inv_m2, ccd_body->mContactNormal, -FLT_MAX, FLT_MAX);" [=[contact_constraint.SolveVelocityConstraintWithMassOverride(body1, inv_m1, body2, inv_m2, ccd_body->mContactNormal, -FLT_MAX, FLT_MAX);
                        if (rs3d_observer::sink) rs3d_observer::sink->resolved(body1.GetID(), body2.GetID(), contact_constraint.GetTotalLambda(), ccd_body->mFraction);]=])
      observer_replace(code "// If the other body moved less than us before hitting something" [=[if (rs3d_observer::sink && ccd_body2 != nullptr && ccd_body2->mFraction < ccd_body->mFraction)
                    rs3d_observer::sink->discarded(body1.GetID(), body2.GetID());
                // If the other body moved less than us before hitting something]=])
      observer_replace(code "// If this is a sensor, we don't want to solve the contact" [=[if (rs3d_observer::sink) rs3d_observer::sink->discarded(body.GetID(), body2.GetID());
                // If this is a sensor, we don't want to solve the contact]=])
    endif()
    set(generated "${observer_dir}/${part}.cpp")
    file(CONFIGURE OUTPUT "${generated}" CONTENT "${code}" @ONLY NEWLINE_STYLE LF)
    file(SHA256 "${generated}" generated_hash)
    string(APPEND audit "${part}.input_sha256=${input_hash}\n${part}.generated_sha256=${generated_hash}\n")
    get_target_property(sources Jolt SOURCES)
    list(FILTER sources EXCLUDE REGEX "/${part}\\.cpp$")
    list(APPEND sources "${generated}")
    set_property(TARGET Jolt PROPERTY SOURCES "${sources}")
  endforeach()
  target_include_directories(Jolt PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/observer")
  string(SHA256 RS3D_OBSERVER_GENERATED_HASH "${audit}")
  file(CONFIGURE OUTPUT "${observer_dir}/audit.ini" CONTENT "${audit}" @ONLY)
endif()
