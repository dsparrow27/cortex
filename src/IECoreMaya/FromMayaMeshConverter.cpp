//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2007-2014, Image Engine Design Inc. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//
//     * Neither the name of Image Engine Design nor the names of any
//       other contributors to this software may be used to endorse or
//       promote products derived from this software without specific prior
//       written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
//  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
//  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
//  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////

#include "IECoreMaya/FromMayaMeshConverter.h"
#include "IECoreMaya/FromMayaPlugConverter.h"
#include "IECoreMaya/MArrayIter.h"
#include "IECoreMaya/VectorTraits.h"

#include "IECore/MeshPrimitive.h"
#include "IECore/VectorOps.h"
#include "IECore/CompoundParameter.h"
#include "IECore/NumericParameter.h"
#include "IECore/MessageHandler.h"
#include "IECore/DespatchTypedData.h"

#include "maya/MFn.h"
#include "maya/MFnMesh.h"
#include "maya/MFnAttribute.h"
#include "maya/MString.h"

#include <algorithm>

using namespace IECoreMaya;
using namespace IECore;
using namespace std;
using namespace Imath;

IE_CORE_DEFINERUNTIMETYPED( FromMayaMeshConverter );

FromMayaShapeConverter::Description<FromMayaMeshConverter> FromMayaMeshConverter::m_description( MFn::kMesh, MeshPrimitiveTypeId, true );
FromMayaShapeConverter::Description<FromMayaMeshConverter> FromMayaMeshConverter::m_dataDescription( MFn::kMeshData, MeshPrimitiveTypeId, true );

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// structors
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FromMayaMeshConverter::FromMayaMeshConverter( const MObject &object )
	:	FromMayaShapeConverter( "Converts poly meshes to IECore::MeshPrimitive objects.", object )
{
	constructCommon();
}

FromMayaMeshConverter::FromMayaMeshConverter( const MDagPath &dagPath )
	:	FromMayaShapeConverter( "Converts poly meshes to IECore::MeshPrimitive objects.", dagPath )
{
	constructCommon();
}

void FromMayaMeshConverter::constructCommon()
{

	// interpolation
	StringParameter::PresetsContainer interpolationPresets;
	interpolationPresets.push_back( StringParameter::Preset( "poly", "linear" ) );
	interpolationPresets.push_back( StringParameter::Preset( "subdiv", "catmullClark" ) );
	// the last interpolation type is 'default'
	interpolationPresets.push_back( StringParameter::Preset( "default", "default" ) );

	StringParameterPtr interpolation = new StringParameter(
		"interpolation",
		"Sets the interpolation type of the new mesh. When 'default' is used it will query the attribute 'ieMeshInterpolation' from the Mesh instead (and use linear if nonexistent).",
		"default",
		interpolationPresets
	);

	parameters()->addParameter( interpolation );

	// colors
	BoolParameterPtr colors = new BoolParameter(
		"colors",
		"When this is on the default color set is added to the result as primitive variable named \"Cs\".",
		false
	);

	parameters()->addParameter( colors );

	// extra colors
	BoolParameterPtr extraColors = new BoolParameter(
		"extraColors",
		"When this is on, all color sets are added to the result as primitive variables named \"setName_Cs\".",
		false
	);

	parameters()->addParameter( extraColors );

}

FromMayaMeshConverter::~FromMayaMeshConverter()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// parameter access
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IECore::StringParameter *FromMayaMeshConverter::interpolationParameter()
{
	return parameters()->parameter< StringParameter >( "interpolation" );
}

const IECore::StringParameter *FromMayaMeshConverter::interpolationParameter() const
{
	return parameters()->parameter< StringParameter >( "interpolation" );
}

IECore::BoolParameter *FromMayaMeshConverter::colorsParameter()
{
	return parameters()->parameter< BoolParameter >( "colors" );
}

const IECore::BoolParameter *FromMayaMeshConverter::colorsParameter() const
{
	return parameters()->parameter< BoolParameter >( "colors" );
}

IECore::BoolParameter *FromMayaMeshConverter::extraColorsParameter()
{
	return parameters()->parameter< BoolParameter >( "extraColors" );
}

const IECore::BoolParameter *FromMayaMeshConverter::extraColorsParameter() const
{
	return parameters()->parameter< BoolParameter >( "extraColors" );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// conversion
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IECore::PrimitiveVariable FromMayaMeshConverter::points() const
{
	MFnMesh fnMesh;
	const MDagPath *d = dagPath( true );
	if( d )
	{
		fnMesh.setObject( *d );
	}
	else
	{
		fnMesh.setObject( object() );
	}

	V3fVectorDataPtr points = new V3fVectorData;
	points->setInterpretation( GeometricData::Point );
	int numVerts = fnMesh.numVertices();
	points->writable().resize( numVerts );

	if( space() == MSpace::kObject )
	{
		const V3f* rawPoints = ( const V3f* )fnMesh.getRawPoints(0);
		copy( rawPoints, rawPoints + numVerts, points->writable().begin() );
	}
	else
	{
		MFloatPointArray mPoints;
		fnMesh.getPoints( mPoints, space() );
		std::transform( MArrayIter<MFloatPointArray>::begin( mPoints ), MArrayIter<MFloatPointArray>::end( mPoints ), points->writable().begin(), VecConvert<MFloatPoint, V3f>() );
	}

	return PrimitiveVariable( PrimitiveVariable::Vertex, points );
}

IECore::PrimitiveVariable FromMayaMeshConverter::normals() const
{
	MFnMesh fnMesh;
	const MDagPath *d = dagPath( true );
	if( d )
	{
		fnMesh.setObject( *d );
	}
	else
	{
		fnMesh.setObject( object() );
	}

	V3fVectorDataPtr normalsData = new V3fVectorData;
	normalsData->setInterpretation( GeometricData::Normal );
	vector<V3f> &normals = normalsData->writable();
	normals.reserve( fnMesh.numFaceVertices() );

	int numPolygons = fnMesh.numPolygons();
	V3f blankVector;

	if( space() == MSpace::kObject )
	{
		const float* rawNormals = fnMesh.getRawNormals(0);
		MIntArray normalIds;
		for( int i=0; i<numPolygons; i++ )
		{
			fnMesh.getFaceNormalIds( i, normalIds );
			for( unsigned j=0; j < normalIds.length(); ++j )
			{
				const float* normalIt = rawNormals + 3 * normalIds[j];
				normals.push_back( blankVector );
				V3f& nn = normals.back();
				nn.x = *normalIt++;
				nn.y = *normalIt++;
				nn.z = *normalIt;
			}
		}
	}
	else
	{
		MFloatVectorArray faceNormals;
		for( int i=0; i<numPolygons; i++ )
		{
			fnMesh.getFaceVertexNormals( i, faceNormals, space() );
			for( unsigned j=0; j<faceNormals.length(); j++ )
			{
				MFloatVector& n = faceNormals[j];
				normals.push_back( blankVector );
				V3f& nn = normals.back();
				nn.x = n.x;
				nn.y = n.y;
				nn.z = n.z;
			}
		}
	}

	return PrimitiveVariable( PrimitiveVariable::FaceVarying, normalsData );
}

IECore::PrimitiveVariable FromMayaMeshConverter::uvs( const MString &uvSet, const std::vector<int> &vertsPerFace ) const
{
	MFnMesh fnMesh( object() );

	IntVectorDataPtr indexData = new IntVectorData;
	vector<int> &indices = indexData->writable();
	indices.reserve( fnMesh.numFaceVertices() );

	// get uv data. A list of uv counts per polygon, and a bunch of uv ids:
	MIntArray uvCounts, uvIds;
	fnMesh.getAssignedUVs( uvCounts, uvIds, &uvSet );

	int uvIdIndex = 0;
	for( size_t i=0; i < vertsPerFace.size(); ++i )
	{
		int numPolyUvs = uvCounts[i];
		int numPolyVerts = vertsPerFace[i];

		if( numPolyUvs == 0 )
		{
			for( int j=0; j < numPolyVerts; ++j )
			{
				indices.push_back( 0 );
			}
		}
		else
		{
			for( int j=0; j < numPolyVerts; ++j )
			{
				indices.push_back( uvIds[ uvIdIndex++ ] );
			}
		}
	}

	V2fVectorDataPtr uvData = new V2fVectorData;
	uvData->setInterpretation( GeometricData::UV );
	std::vector<Imath::V2f> &uvs = uvData->writable();

	MFloatArray uArray, vArray;
	fnMesh.getUVs( uArray, vArray, &uvSet );

	size_t numIndices = indices.size();
	if( uArray.length() == 0 )
	{
		uvs.resize( numIndices, Imath::V2f( .0f ) );
	}
	else
	{
		uvs.reserve( uArray.length() );
		for( size_t i=0; i < uArray.length(); ++i )
		{
			uvs.emplace_back( uArray[i], vArray[i] );
		}
	}

	return PrimitiveVariable( PrimitiveVariable::FaceVarying, uvData, indexData );
}

IECore::PrimitiveVariable FromMayaMeshConverter::colors( const MString &colorSet, bool forceRgb ) const
{
	MFnMesh fnMesh( object() );
	MFnMesh::MColorRepresentation rep = fnMesh.getColorRepresentation( colorSet );

	int numColors = fnMesh.numFaceVertices();
	MColorArray colors;
	MColor defaultColor(0,0,0,1);
	if ( !fnMesh.getFaceVertexColors( colors, &colorSet, &defaultColor ) )
	{
		throw Exception( ( boost::format( "Failed to obtain colors from color set '%s'" ) % colorSet ).str() );
	}

	int availableColors = colors.length();
	if ( availableColors > numColors )
	{
		availableColors = numColors;
	}

	DataPtr data;

	if ( rep == MFnMesh::kAlpha )
	{
		if ( forceRgb )
		{
			Color3fVectorDataPtr colorVec = new Color3fVectorData();
			colorVec->writable().resize( numColors, Imath::Color3f(1) );
			std::vector< Imath::Color3f >::iterator it = colorVec->writable().begin();
			for ( int i = 0; i < availableColors; i++, it++ )
			{
				*it = Imath::Color3f( colors[i][3] );
			}
			data = colorVec;
		}
		else
		{
			FloatVectorDataPtr colorVec = new FloatVectorData();
			colorVec->writable().resize( numColors, 1 );
			std::vector< float >::iterator it = colorVec->writable().begin();
			for ( int i = 0; i < availableColors; i++, it++ )
			{
				*it = colors[i][3];
			}
			data = colorVec;
		}
	}
	else
	{
		if ( rep == MFnMesh::kRGB || forceRgb )
		{
			Color3fVectorDataPtr colorVec = new Color3fVectorData();
			colorVec->writable().resize( numColors, Imath::Color3f(0,0,0) );
			std::vector< Imath::Color3f >::iterator it = colorVec->writable().begin();
			for ( int i = 0; i < availableColors; i++, it++ )
			{
				const MColor &c = colors[i];
				*it = Imath::Color3f( c[0], c[1], c[2] );
			}
			data = colorVec;
		}
		else
		{
			Color4fVectorDataPtr colorVec = new Color4fVectorData();
			colorVec->writable().resize( numColors, Imath::Color4f(0,0,0,1) );
			std::vector< Imath::Color4f >::iterator it = colorVec->writable().begin();
			for ( int i = 0; i < availableColors; i++, it++ )
			{
				const MColor &c = colors[i];
				*it = Imath::Color4f( c[0], c[1], c[2], c[3] );
			}
			data = colorVec;
		}
	}
	return PrimitiveVariable( PrimitiveVariable::FaceVarying, data );
}

IECore::PrimitivePtr FromMayaMeshConverter::doPrimitiveConversion( const MObject &object, IECore::ConstCompoundObjectPtr operands ) const
{
	MFnMesh fnMesh( object );
	return doPrimitiveConversion( fnMesh );
}

IECore::PrimitivePtr FromMayaMeshConverter::doPrimitiveConversion( const MDagPath &dagPath, IECore::ConstCompoundObjectPtr operands ) const
{
	MFnMesh fnMesh( dagPath );
	return doPrimitiveConversion( fnMesh );
}

IECore::PrimitivePtr FromMayaMeshConverter::doPrimitiveConversion( MFnMesh &fnMesh ) const
{
	// get basic topology and create a mesh
	int numPolygons = fnMesh.numPolygons();
	IntVectorDataPtr verticesPerFaceData = new IntVectorData;
	verticesPerFaceData->writable().resize( numPolygons );
	vector<int>::iterator verticesPerFaceIt = verticesPerFaceData->writable().begin();

	IntVectorDataPtr vertexIds = new IntVectorData;
	// We are calling fnMesh.numFaceVertices() twice to work around a known bug in Maya. When accessing
	// certain MFnMesh API calls, given a mesh with 6 or more UV sets, which has never been evaluated
	// before, the first call returns 0 and kFailure, and the second call works as expected.
	// See ToMayaMeshConverterTest.testManyUVConversionsFromPlug for an example of how this might occur.
	fnMesh.numFaceVertices();
	vertexIds->writable().resize( fnMesh.numFaceVertices() );
	vector<int>::iterator vertexIdsIt = vertexIds->writable().begin();

	MIntArray vertexCounts, polygonVertices;
	fnMesh.getVertices( vertexCounts, polygonVertices );

	copy( MArrayIter<MIntArray>::begin( vertexCounts ), MArrayIter<MIntArray>::end( vertexCounts ), verticesPerFaceIt );
	copy( MArrayIter<MIntArray>::begin( polygonVertices ), MArrayIter<MIntArray>::end( polygonVertices ), vertexIdsIt );

	std::string interpolation = interpolationParameter()->getTypedValue();
	if ( interpolation == "default" )
	{
		MStatus st;
		MPlug interpolationPlug = fnMesh.findPlug( "ieMeshInterpolation", &st );
		if ( st )
		{
			unsigned int interpolationIndex = interpolationPlug.asInt(MDGContext::fsNormal, &st);
			if ( st )
			{
				if ( interpolationIndex < interpolationParameter()->getPresets().size() - 1 )
				{
					// convert interpolation index to the preset value
					interpolation = boost::static_pointer_cast< StringData >( interpolationParameter()->getPresets()[interpolationIndex].second )->readable();
				}
				else
				{
					interpolation = "linear";
				}
			}
			else
			{
				interpolation = "linear";
			}
		}
		else
		{
			interpolation = "linear";
		}
	}

	MeshPrimitivePtr result = new MeshPrimitive( verticesPerFaceData, vertexIds, interpolation );

	result->variables["P"] = points();

	if( interpolation=="linear" )
	{
		result->variables["N"] = normals();
	}

	MString currentUVSet;
	fnMesh.getCurrentUVSetName( currentUVSet );
	if( currentUVSet.length() )
	{
		result->variables["uv"] = uvs( currentUVSet, verticesPerFaceData->readable() );
	}

	MStringArray uvSets;
	fnMesh.getUVSetNames( uvSets );
	for( unsigned int i=0; i<uvSets.length(); i++ )
	{
		if( uvSets[i] == currentUVSet )
		{
			// we've already converted these UVs above
			continue;
		}

		result->variables[ uvSets[i].asChar() ] = uvs( uvSets[i], verticesPerFaceData->readable() );
	}

	bool convertColors = colorsParameter()->getTypedValue();
	bool convertExtraColors = extraColorsParameter()->getTypedValue();
	if ( convertColors || convertExtraColors )
	{
		MString currentColorSet;
		fnMesh.getCurrentColorSetName( currentColorSet );
		MStringArray colorSets;
		fnMesh.getColorSetNames( colorSets );
		for( unsigned int i=0; i<colorSets.length(); i++ )
		{
			if( convertColors && colorSets[i]==currentColorSet )
			{
				// Cs is always converted to Color3f
				result->variables["Cs"] = colors( currentColorSet, true );
			}

			if( convertExtraColors )
			{
				MString sName = colorSets[i] + "_Cs";
				// Extra color sets are not converted
				result->variables[sName.asChar()] = colors( colorSets[i] );
			}
		}
	}

	return result;
}
