#ifndef Magnum_Trade_OpenDdlValidation_h
#define Magnum_Trade_OpenDdlValidation_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015
              Vladimír Vondruš <mosra@centrum.cz>

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

/** @file
 * @brief Class @ref Magnum::OpenDdl::Validation::Property, @ref Magnum::OpenDdl::Validation::Structure, typedef @ref Magnum::OpenDdl::Validation::Primitives, @ref Magnum::OpenDdl::Validation::Properties, @ref Magnum::OpenDdl::Validation::Structures, tag @ref Magnum::OpenDdl::Validation::RequiredPropertyType, constant @ref Magnum::OpenDdl::Validation::OptionalProperty, @ref Magnum::OpenDdl::Validation::RequiredProperty
 */

#include <Corrade/Containers/Array.h>

#include "MagnumPlugins/OpenGexImporter/OpenDdl/Type.h"

namespace Magnum { namespace OpenDdl {

/**
@brief OpenDDL document validation

See @ref Document::validate() for more information.
*/
namespace Validation {

/**
@brief Tag type for required and optional properties

@see @ref Property, @ref RequiredProperty, @ref OptionalProperty
*/
struct RequiredPropertyType {
    #ifndef DOXYGEN_GENERATING_OUTPUT
    bool required;
    #endif
};

/**
@brief Required property

@see @ref Property
*/
constexpr RequiredPropertyType RequiredProperty{true};

/**
@brief Optional property

@see @ref Property
*/
constexpr RequiredPropertyType OptionalProperty{false};

/**
@brief Property specification

Example usage (excerpt from OpenGEX specification of `Animation` structure):
@code
Properties{{clip, PropertyType::UnsignedInt, OptionalProperty},
           {begin, PropertyType::Float, OptionalProperty},
           {end, PropertyType::Float, OptionalProperty}}
@endcode

@see @ref Properties
*/
class Property {
    public:
        /**
         * @brief Constructor
         * @param identifier    Property identifier
         * @param type          Expected property type
         * @param required      Whether the property is required
         */
        constexpr /*implicit*/ Property(Int identifier, PropertyType type, RequiredPropertyType required): _identifier{identifier}, _type{type}, _required{required.required} {}

        #ifndef DOXYGEN_GENERATING_OUTPUT
        constexpr Int identifier() const { return _identifier; }
        constexpr PropertyType type() const { return _type; }
        constexpr bool isRequired() const { return _required; }
        #endif

    private:
        /** @todo remove this ugly hack when we have Containers::Array(NoInit) */
        /* GCC 4.6 needs the class keyword */
        friend class Containers::Array<Property>;
        constexpr explicit Property(): _identifier{-1}, _type{}, _required{} {}

        Int _identifier;
        PropertyType _type;
        bool _required;
};

/**
@brief List of allowed properties for validation

See @ref Validation::Property for example usage.
*/
typedef std::initializer_list<Property> Properties;

/**
@brief List of allowed structures for validation

First value is structure identifier, the pair specifies minimal and maximal
allowed count of structures with given identifier. Maximal count set to `0`
means that there is no upper limit.

See @ref Validation::Structure for example usage.
*/
typedef std::initializer_list<std::pair<Int, std::pair<Int, Int>>> Structures;

/**
@brief List of allowed primitive types for validation

See @ref Validation::Structure for example usage.
*/
typedef std::initializer_list<Type> Primitives;

/**
@brief Structure spec for validation

Example usage (excerpt from OpenGEX specification of `Texture` structure):
@code
{Texture,
    // Requiring string attrib property, optional integer texcoord property
    Properties{{attrib, PropertyType::String, RequiredProperty},
               {texcoord, PropertyType::UnsignedInt, OptionalProperty}},

    // Requiring exactly one primitive substructure with exactly one string
    // value for filename
    Primitives{Type::String}, 1, 1,

    // There can be any number of Transform, Translation, Rotation, Scale and
    // Animation substructures
    Structures{{Transform, {}},
               {Translation, {}},
               {Rotation, {}},
               {Scale, {}},
               {Animation, {}}}}
@endcode
*/
class Structure {
    public:
        /**
         * @brief Constructor
         * @param identifier            Structure identifier
         * @param properties            List of allowed properties
         * @param primitives            List of allowed primitive types
         * @param structures            List of allowed custom sub-structures
         * @param primitiveCount        Expected primitive sub-structure count
         * @param primitiveArraySize    Expected primitive array size
         *
         * Setting @p primitiveCount to `0` means that there is no requirement
         * on primitive array count. Setting @p primitiveArraySize to `0` means
         * that there is no requirement on primitive array size.
         */
        /* GCC 4.6 doesn't have delegating constructors, also there's some
           problem with the default value */
        /*implicit*/ Structure(Int identifier, Properties properties, Primitives primitives, std::size_t primitiveCount, std::size_t primitiveArraySize, Structures structures = Structures{}): _identifier{identifier}, _properties{properties.size()}, _primitives{primitives.size()}, _structures{structures.size()}, _primitiveCount{primitiveCount}, _primitiveArraySize{primitiveArraySize} {
            initialize(properties, primitives, structures);
        }

        /** @overload */
        /* GCC 4.6 doesn't have delegating constructors, also there's some
           problem with the default value */
        /*implicit*/ Structure(Int identifier, Primitives primitives, std::size_t primitiveCount, std::size_t primitiveArraySize, Structures structures = Structures{}): _identifier{identifier}, _primitives{primitives.size()}, _structures{structures.size()}, _primitiveCount{primitiveCount}, _primitiveArraySize{primitiveArraySize} {
            initialize({}, primitives, structures);
        }

        /** @overload */
        /* GCC 4.6 doesn't have delegating constructors, also there's some
           problem with the default value */
        /*implicit*/ Structure(Int identifier, Properties properties, Structures structures = Structures{}): _identifier{identifier}, _properties{properties.size()}, _structures{structures.size()}, _primitiveCount{}, _primitiveArraySize{} {
            initialize(properties, {}, structures);
        }

        /** @overload */
        /* GCC 4.6 doesn't have delegating constructors, also there's some
           problem with the default value */
        /*implicit*/ Structure(Int identifier, Structures structures = Structures{}): _identifier{identifier}, _structures{structures.size()}, _primitiveCount{}, _primitiveArraySize{} {
            initialize({}, {}, structures);
        }

        #ifndef DOXYGEN_GENERATING_OUTPUT
        Int identifier() const { return _identifier; }
        Containers::ArrayView<const Property> properties() const { return _properties; }
        Containers::ArrayView<const Type> primitives() const { return _primitives; }
        Containers::ArrayView<const std::pair<Int, std::pair<Int, Int>>> structures() const { return _structures; }
        std::size_t primitiveCount() const { return _primitiveCount; }
        std::size_t primitiveArraySize() const { return _primitiveArraySize; }
        #endif

    private:
        void initialize(Properties properties, Primitives primitives, Structures structures);

        Int _identifier;

        Containers::Array<Property> _properties;
        Containers::Array<Type> _primitives;
        Containers::Array<std::pair<Int, std::pair<Int, Int>>> _structures;
        std::size_t _primitiveCount;
        std::size_t _primitiveArraySize;
};

}}}

#endif
