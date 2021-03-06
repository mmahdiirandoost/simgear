
include (SimGearComponent)

set(HEADERS 
    AtomicChangeListener.hxx
    ExtendedPropertyAdapter.hxx
    PropertyBasedElement.hxx
    PropertyBasedMgr.hxx
    condition.hxx
    propertyObject.hxx
    props.hxx
    props_io.hxx
    propsfwd.hxx
    tiedpropertylist.hxx
    vectorPropTemplates.hxx
    )

set(SOURCES 
    AtomicChangeListener.cxx
    PropertyBasedElement.cxx
    PropertyBasedMgr.cxx
    condition.cxx
    propertyObject.cxx
    props.cxx
    props_io.cxx
    )

simgear_component(props props "${SOURCES}" "${HEADERS}")

if(ENABLE_TESTS)
add_executable(test_props props_test.cxx)
target_link_libraries(test_props SimGearCore)
add_test(test_props ${EXECUTABLE_OUTPUT_PATH}/test_props)

add_executable(test_propertyObject propertyObject_test.cxx)
target_link_libraries(test_propertyObject SimGearCore)
add_test(test_propertyObject ${EXECUTABLE_OUTPUT_PATH}/test_propertyObject)
endif(ENABLE_TESTS)
