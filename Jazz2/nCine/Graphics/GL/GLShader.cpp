#include "GLShader.h"
#include "../../IO/IFileStream.h"

#include <string>

#if defined(__EMSCRIPTEN__) || defined(WITH_ANGLE)
#include "../../Application.h"
#endif

namespace nCine {

	static std::string patchLines;

	///////////////////////////////////////////////////////////
	// CONSTRUCTORS and DESTRUCTOR
	///////////////////////////////////////////////////////////

	GLShader::GLShader(GLenum type)
		: glHandle_(0), status_(Status::NOT_COMPILED)
	{
		if (patchLines.empty()) {
#if (defined(WITH_OPENGLES) && GL_ES_VERSION_3_0) || defined(__EMSCRIPTEN__)
			patchLines.append("#version 300 es\n");
#else
			patchLines.append("#version 330\n");
#endif

#if defined(__EMSCRIPTEN__) || defined(WITH_ANGLE)
			// ANGLE does not seem capable of handling large arrays that are not entirely filled.
			// A small array size will also make shader compilation a lot faster.
			if (theApplication().appConfiguration().fixedBatchSize > 0) {
				patchLines.append("#define WITH_FIXED_BATCH_SIZE\n");
				//patchLines.formatAppend("#define BATCH_SIZE (%u)\n", theApplication().appConfiguration().fixedBatchSize);
				patchLines.append("#define BATCH_SIZE (");
				patchLines.append(std::to_string(theApplication().appConfiguration().fixedBatchSize));
				patchLines.append(")\n");
			}
#endif
		}

		glHandle_ = glCreateShader(type);
	}

	GLShader::GLShader(GLenum type, const char* filename)
		: GLShader(type)
	{
		loadFromFile(filename);
	}

	GLShader::~GLShader()
	{
		glDeleteShader(glHandle_);
	}

	///////////////////////////////////////////////////////////
	// PUBLIC FUNCTIONS
	///////////////////////////////////////////////////////////

	void GLShader::loadFromString(const char* string)
	{
		////ASSERT(string);

		const GLchar* source_lines[2] = { patchLines.data(), string };
		glShaderSource(glHandle_, 2, source_lines, nullptr);
	}

	void GLShader::loadFromFile(const char* filename)
	{
		std::unique_ptr<IFileStream> fileHandle = IFileStream::createFileHandle(filename);

		fileHandle->Open(FileAccessMode::Read);
		if (fileHandle->isOpened()) {
			const GLint length = static_cast<int>(fileHandle->GetSize());
			std::string source(length, '\0');
			fileHandle->Read(source.data(), length);

			const GLchar* source_lines[2] = { patchLines.data(), source.data() };
			const GLint lengths[2] = { static_cast<GLint>(patchLines.length()), length };
			glShaderSource(glHandle_, 2, source_lines, lengths);
		}
	}

	void GLShader::compile(ErrorChecking errorChecking)
	{
		glCompileShader(glHandle_);

		if (errorChecking == ErrorChecking::IMMEDIATE)
			checkCompilation();
		else
			status_ = Status::COMPILED_WITH_DEFERRED_CHECKS;
	}

	bool GLShader::checkCompilation()
	{
		if (status_ == Status::COMPILED)
			return true;

		GLint status = 0;
		glGetShaderiv(glHandle_, GL_COMPILE_STATUS, &status);
		if (status == GL_FALSE) {
			GLint length = 0;
			glGetShaderiv(glHandle_, GL_INFO_LOG_LENGTH, &length);

			if (length > 0) {
				std::string infoLog(length, '\0');
				glGetShaderInfoLog(glHandle_, length, &length, infoLog.data());
				//LOGW_X("%s", infoLog.data());
			}

			status_ = Status::COMPILATION_FAILED;
			return false;
		}

		status_ = Status::COMPILED;
		return true;
	}

}
