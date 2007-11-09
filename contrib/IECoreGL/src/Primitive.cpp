//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2007, Image Engine Design Inc. All rights reserved.
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

#include "IECoreGL/Primitive.h"
#include "IECoreGL/State.h"
#include "IECoreGL/Exception.h"
#include "IECoreGL/TypedStateComponent.h"
#include "IECoreGL/ShaderStateComponent.h"
#include "IECoreGL/Shader.h"
#include "IECoreGL/TextureUnits.h"
#include "IECoreGL/NumericTraits.h"

#include "IECore/TypedDataDespatch.h"
#include "IECore/VectorTypedData.h"

#include "boost/format.hpp"

using namespace IECoreGL;
using namespace std;
using namespace boost;
using namespace Imath;

Primitive::Primitive()
{
}

Primitive::~Primitive()
{
}
	
void Primitive::render( ConstStatePtr state ) const
{
	if( !state->isComplete() )
	{
		throw Exception( "Primitive::render called with incomplete state object." );
	}
	
	Shader *shader = const_cast<Shader *>( state->get<ShaderStateComponent>()->shader().get() );
	// get ready in case the derived class wants to call setVertexAttributesAsUniforms
	setupVertexAttributesAsUniform( shader );

	glPushAttrib( GL_TEXTURE_BIT | GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT | GL_POLYGON_BIT | GL_LINE_BIT | GL_LIGHTING_BIT );
	glPushClientAttrib( GL_CLIENT_VERTEX_ARRAY_BIT );
	
		if( depthSortRequested( state ) )
		{
			glDepthMask( false );
		}

		if( state->get<PrimitiveSolid>()->value() )
		{
			glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
			glEnable( GL_LIGHTING );
			glDisable( GL_POLYGON_OFFSET_FILL );
			render( state, PrimitiveSolid::staticTypeId() );
		}

		glDisable( GL_LIGHTING );
		glActiveTexture( textureUnits()[0] );
		glDisable( GL_TEXTURE_2D );
		GLint program = 0;
		// annoyingly i can't find a way of pushing and popping the
		// current program so we have to do that ourselves
		if( GLEW_VERSION_2_1 )
		{
			glGetIntegerv( GL_CURRENT_PROGRAM, &program );
		}
		if( program )
		{
			glUseProgram( 0 );
		}
		
			if( state->get<PrimitiveOutline>()->value() )
			{
				glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
				glEnable( GL_POLYGON_OFFSET_LINE );
				float width = 2 * state->get<PrimitiveOutlineWidth>()->value();
				glPolygonOffset( 2 * width, 1 );
				glLineWidth( width );
				Color4f c = state->get<OutlineColorStateComponent>()->value();
				glColor4f( c[0], c[1], c[2], c[3] );
				render( state, PrimitiveOutline::staticTypeId() );
			}

			if( state->get<PrimitiveWireframe>()->value() )
			{
				glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
				float width = state->get<PrimitiveWireframeWidth>()->value();
				glEnable( GL_POLYGON_OFFSET_LINE );
				glPolygonOffset( -1 * width, -1 );
				Color4f c = state->get<WireframeColorStateComponent>()->value();
				glColor4f( c[0], c[1], c[2], c[3] );
				glLineWidth( width );
				render( state, PrimitiveWireframe::staticTypeId() );
			}

			if( state->get<PrimitivePoints>()->value() )
			{
				glPolygonMode( GL_FRONT_AND_BACK, GL_POINT );
				float width = state->get<PrimitivePointWidth>()->value();
				glEnable( GL_POLYGON_OFFSET_POINT );
				glPolygonOffset( -2 * width, -1 );
				glPointSize( width );
				Color4f c = state->get<PointColorStateComponent>()->value();
				glColor4f( c[0], c[1], c[2], c[3] );
				render( state, PrimitivePoints::staticTypeId() );
			}
			
			if( state->get<PrimitiveBound>()->value() )
			{
				Box3f b = bound();
				Color4f c = state->get<BoundColorStateComponent>()->value();
				glColor4f( c[0], c[1], c[2], c[3] );
				glLineWidth( 1 );
				glBegin( GL_LINE_LOOP );
					glVertex3f( b.min.x, b.min.y, b.min.z );
					glVertex3f( b.max.x, b.min.y, b.min.z );
					glVertex3f( b.max.x, b.max.y, b.min.z );
					glVertex3f( b.min.x, b.max.y, b.min.z );
				glEnd();
				glBegin( GL_LINE_LOOP );
					glVertex3f( b.min.x, b.min.y, b.max.z );
					glVertex3f( b.max.x, b.min.y, b.max.z );
					glVertex3f( b.max.x, b.max.y, b.max.z );
					glVertex3f( b.min.x, b.max.y, b.max.z );
				glEnd();
				glBegin( GL_LINES );
					glVertex3f( b.min.x, b.min.y, b.min.z );
					glVertex3f( b.min.x, b.min.y, b.max.z );
					glVertex3f( b.max.x, b.min.y, b.min.z );
					glVertex3f( b.max.x, b.min.y, b.max.z );
					glVertex3f( b.max.x, b.max.y, b.min.z );
					glVertex3f( b.max.x, b.max.y, b.max.z );
					glVertex3f( b.min.x, b.max.y, b.min.z );
					glVertex3f( b.min.x, b.max.y, b.max.z );
				glEnd();
			}
			
		if( program )
		{
			glUseProgram( program );
		}
		
	glPopClientAttrib();
	glPopAttrib();
}

size_t Primitive::vertexAttributeSize() const
{
	return 0;
}

void Primitive::addVertexAttribute( const std::string &name, IECore::ConstDataPtr data )
{
	if( !vertexAttributeSize() )
	{
		throw Exception( typeName() + " does not support vertex attributes." );
	}
	
	size_t s = 0;
	try
	{
		s = IECore::despatchVectorTypedDataFn<size_t, IECore::VectorTypedDataSize, IECore::VectorTypedDataSizeArgs>( const_pointer_cast<IECore::Data>( data ), IECore::VectorTypedDataSizeArgs() );
	}
	catch( const std::exception &e )
	{
		throw Exception( "Data provided is not suitable for use as a vertex attribute." );
	}

	size_t rightSize = vertexAttributeSize();
	if( s!=rightSize )
	{
		throw Exception( boost::str( format( "Vertex attribute \"%s\" has wrong number of elements (%d but expected %d)." ) % name % s % rightSize ) );
	}
	
	m_vertexAttributes[name] = data->copy();
}

struct SetVertexAttributeArgs
{
	GLint vertexArrayIndex;
};

template<typename T>
struct SetVertexAttribute
{
	const void operator() ( typename T::Ptr data, const SetVertexAttributeArgs &args )
	{
		GLenum type = NumericTraits<typename T::BaseType>::glType();
		int elementSize = data->baseSize() / data->readable().size();
		glEnableVertexAttribArray( args.vertexArrayIndex );
		glVertexAttribPointer( args.vertexArrayIndex, elementSize, type, false, 0, data->baseReadable() );
	};
};

void Primitive::setVertexAttributes( ConstStatePtr state ) const
{
	if( !m_vertexAttributes.size() )
	{
		return;
	}
	
	Shader *shader = const_cast<Shader *>( state->get<ShaderStateComponent>()->shader().get() );
	if( !shader )
	{
		return;
	}

	GLint numAttributes = 0;
	glGetProgramiv( shader->m_program, GL_ACTIVE_ATTRIBUTES, &numAttributes );
	GLint maxAttributeNameLength = 0;
	glGetProgramiv( shader->m_program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxAttributeNameLength ); 
	vector<char> name; name.resize( maxAttributeNameLength );
	for( int i=0; i<numAttributes; i++ )
	{
		GLint size = 0;
		GLenum type = 0;
		glGetActiveAttrib( shader->m_program, i, maxAttributeNameLength, 0, &size, &type, &name[0] );
		if( size==1 )
		{
			VertexAttributeMap::const_iterator it = m_vertexAttributes.find( &name[0] );
			if( it!=m_vertexAttributes.end() )
			{
				SetVertexAttributeArgs a;
				a.vertexArrayIndex = glGetAttribLocation( shader->m_program, &name[0] );
				IECore::despatchVectorTypedDataFn<void, SetVertexAttribute>( const_pointer_cast<IECore::Data>( it->second ), a );
			}
		}
	}
}

void Primitive::setVertexAttributesAsUniforms( unsigned int vertexIndex ) const
{
	if( !m_vertexToUniform.shader )
	{
		return;
	}
	std::map<GLint, const int *>::const_iterator it;
	for( it=m_vertexToUniform.intDataMap.begin(); it!=m_vertexToUniform.intDataMap.end(); it++ )
	{
		m_vertexToUniform.shader->setParameter( it->first, it->second[vertexIndex] );
	}
}

void Primitive::setupVertexAttributesAsUniform( Shader *s ) const
{
	if( !s )
	{
		m_vertexToUniform.shader = 0;
		return;
	}
	
	if( s==m_vertexToUniform.shader && (*s)==(*m_vertexToUniform.shader) )
	{
		return;
	}

	m_vertexToUniform.intDataMap.clear();
	
	for( VertexAttributeMap::const_iterator it=m_vertexAttributes.begin(); it!=m_vertexAttributes.end(); it++ )
	{
		try
		{
			GLint parameterIndex = s->parameterIndex( it->first );
			switch( it->second->typeId() )
			{
				case IECore::IntVectorDataTypeId :
					m_vertexToUniform.intDataMap[parameterIndex] = &(static_pointer_cast<const IECore::IntVectorData>( it->second )->readable()[0]);
					break;
				default :
					/// \todo The other types
					break;
			}
		}
		catch( ... )
		{
		}
	}
	
	m_vertexToUniform.shader = s;
}

bool Primitive::depthSortRequested( ConstStatePtr state ) const
{
	return state->get<PrimitiveTransparencySortStateComponent>()->value() &&
		state->get<TransparentShadingStateComponent>()->value();
}
