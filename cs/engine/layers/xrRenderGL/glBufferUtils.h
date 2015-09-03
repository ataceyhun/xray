#pragma once

#ifdef USE_OGL

namespace glBufferUtils
{
	void	CreateVertexBuffer(GLuint* pBuffer, const void* pData, UINT DataSize, bool bImmutable = true);
	void	CreateIndexBuffer(GLuint* pBuffer, const void* pData, UINT DataSize, bool bImmutable = true);
	GLsizei GetFVFVertexSize(u32 FVF);
	GLsizei GetDeclVertexSize(const D3DVERTEXELEMENT9* decl);
	u32		GetDeclLength(const D3DVERTEXELEMENT9* decl);
	void	ConvertVertexDeclaration(u32 FVF, GLuint vao = 0);
	void	ConvertVertexDeclaration(const D3DVERTEXELEMENT9* decl, GLuint vao = 0);
};

#endif // USE_OGL
