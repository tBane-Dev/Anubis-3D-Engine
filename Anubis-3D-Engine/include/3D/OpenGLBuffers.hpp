#pragma once

#include <SFML/OpenGL.hpp>
#include <SFML/Window/Context.hpp>
#include <cstddef>

namespace Anubis::GL {
	using GenBuffersFunction = void (APIENTRY*)(GLsizei, GLuint*);
	using BindBufferFunction = void (APIENTRY*)(GLenum, GLuint);
	using BufferDataFunction = void (APIENTRY*)(GLenum, std::ptrdiff_t, const void*, GLenum);
	using DeleteBuffersFunction = void (APIENTRY*)(GLsizei, const GLuint*);

	inline GenBuffersFunction genBuffers = nullptr;
	inline BindBufferFunction bindBuffer = nullptr;
	inline BufferDataFunction bufferData = nullptr;
	inline DeleteBuffersFunction deleteBuffers = nullptr;

	inline constexpr GLenum ArrayBuffer = 0x8892;
	inline constexpr GLenum ElementArrayBuffer = 0x8893;
	inline constexpr GLenum StaticDraw = 0x88E4;

	//

	using GenFramebuffersFunction = void (APIENTRY*)(GLsizei, GLuint*);
	using BindFramebufferFunction = void (APIENTRY*)(GLenum, GLuint);
	using FramebufferTexture2DFunction = void (APIENTRY*)(GLenum, GLenum, GLenum, GLuint, GLint);
	using CheckFramebufferStatusFunction = GLenum(APIENTRY*)(GLenum);
	using DeleteFramebuffersFunction = void (APIENTRY*)(GLsizei, const GLuint*);

	inline GenFramebuffersFunction genFramebuffers = nullptr;
	inline BindFramebufferFunction bindFramebuffer = nullptr;
	inline FramebufferTexture2DFunction framebufferTexture2D = nullptr;
	inline CheckFramebufferStatusFunction checkFramebufferStatus = nullptr;
	inline DeleteFramebuffersFunction deleteFramebuffers = nullptr;

	inline constexpr GLenum Framebuffer = 0x8D40;
	inline constexpr GLenum ColorAttachment0 = 0x8CE0;
	inline constexpr GLenum DepthAttachment = 0x8D00;
	inline constexpr GLenum FramebufferComplete = 0x8CD5;

	//

	using GenQueriesFunction = void (APIENTRY*)(GLsizei, GLuint*);
	using DeleteQueriesFunction = void (APIENTRY*)(GLsizei, const GLuint*);
	using BeginQueryFunction = void (APIENTRY*)(GLenum, GLuint);
	using EndQueryFunction = void (APIENTRY*)(GLenum);
	using GetQueryObjectuivFunction = void (APIENTRY*)(GLuint, GLenum, GLuint*);

	inline GenQueriesFunction genQueries = nullptr;
	inline DeleteQueriesFunction deleteQueries = nullptr;
	inline BeginQueryFunction beginQuery = nullptr;
	inline EndQueryFunction endQuery = nullptr;
	inline GetQueryObjectuivFunction getQueryObjectuiv = nullptr;

	inline constexpr GLenum AnySamplesPassed = 0x8C2F;
	inline constexpr GLenum QueryResult = 0x8866;
	inline constexpr GLenum QueryResultAvailable = 0x8867;

	//

	using GenVertexArraysFunction = void (APIENTRY*)(GLsizei, GLuint*);
	using BindVertexArrayFunction = void (APIENTRY*)(GLuint);
	using DeleteVertexArraysFunction = void (APIENTRY*)(GLsizei, const GLuint*);

	inline GenVertexArraysFunction genVertexArrays = nullptr;
	inline BindVertexArrayFunction bindVertexArray = nullptr;
	inline DeleteVertexArraysFunction deleteVertexArrays = nullptr;

	using EnableVertexAttribArrayFunction = void (APIENTRY*)(GLuint);
	using VertexAttribPointerFunction = void (APIENTRY*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
	using VertexAttribIPointerFunction = void (APIENTRY*)(GLuint, GLint, GLenum, GLsizei, const void*);

	inline EnableVertexAttribArrayFunction enableVertexAttribArray = nullptr;
	inline VertexAttribPointerFunction vertexAttribPointer = nullptr;
	inline VertexAttribIPointerFunction vertexAttribIPointer = nullptr;

	//

	using ActiveTextureFunction = void (APIENTRY*)(GLenum);
	using UseProgramFunction = void (APIENTRY*)(GLuint);

	inline ActiveTextureFunction activeTexture = nullptr;
	inline UseProgramFunction useProgram = nullptr;

	//
	using TexBufferFunction =
		void (APIENTRY*)(GLenum, GLenum, GLuint);

	inline TexBufferFunction texBuffer = nullptr;

	inline constexpr GLenum TextureBuffer = 0x8C2A;
	inline constexpr GLenum TextureBindingBuffer = 0x8C2C;
	inline constexpr GLenum MaxTextureBufferSize = 0x8C2B;
	inline constexpr GLenum RGBA32F = 0x8814;
	inline constexpr GLenum DynamicDraw = 0x88E8;


	bool initialize();
}