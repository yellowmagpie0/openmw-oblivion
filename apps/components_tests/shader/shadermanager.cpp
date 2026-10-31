#include <components/files/conversion.hpp>
#include <components/shader/shadermanager.hpp>
#include <components/shader/shadervisitor.hpp>
#include <components/resource/imagemanager.hpp>
#include <components/sceneutil/texturetype.hpp>
#include <osg/Geometry>
#include <osg/Texture2D>
#include <components/testing/util.hpp>

#include <fstream>

#include <gtest/gtest.h>

namespace
{
    using namespace testing;
    using namespace Shader;

    struct ShaderManagerTest : Test
    {
        ShaderManager mManager;
        ShaderManager::DefineMap mDefines;

        ShaderManagerTest() { mManager.setShaderPath(TestingOpenMW::outputDir()); }

        template <class F>
        void withShaderFile(const std::string& content, F&& f)
        {
            withShaderFile("", content, std::forward<F>(f));
        }

        template <class F>
        void withShaderFile(const std::string& suffix, const std::string& content, F&& f)
        {
            auto subdir = std::filesystem::path("lib")
                / std::filesystem::path(
                    std::string(UnitTest::GetInstance()->current_test_info()->name()) + suffix + ".vert");
            auto path = TestingOpenMW::outputFilePathWithSubDir(subdir);
            {
                std::ofstream stream(path);
                stream << content;
                stream.close();
            }

            f(subdir);
        }
    };

    TEST_F(ShaderManagerTest, get_shader_with_empty_content_should_succeed)
    {
        const std::string content;

        withShaderFile(content, [this](const std::filesystem::path& templateName) {
            EXPECT_TRUE(mManager.getShader(Files::pathToUnicodeString(templateName)));
        });
    }

    TEST_F(ShaderManagerTest, get_shader_should_not_change_source_without_template_parameters)
    {
        const std::string content
            = "#version 120\n"
              "void main() {}\n";

        withShaderFile(content, [&](const std::filesystem::path& templateName) {
            const auto shader = mManager.getShader(Files::pathToUnicodeString(templateName), mDefines);
            ASSERT_TRUE(shader);
            EXPECT_EQ(shader->getShaderSource(), content);
        });
    }

    TEST_F(ShaderManagerTest, get_shader_should_replace_includes_with_content)
    {
        const std::string content0 = "void foo() {}\n";

        withShaderFile("_0", content0, [&](const std::filesystem::path& templateName0) {
            const std::string content1 =
                "#include \"" + Files::pathToUnicodeString(templateName0) + "\"\n"
                "void bar() { foo() }\n";

            withShaderFile("_1", content1, [&](const std::filesystem::path& templateName1) {
                const std::string content2 =
                    "#version 120\n"
                    "#include \"" + Files::pathToUnicodeString(templateName1) + "\"\n"
                    "void main() { bar() }\n";

                withShaderFile(content2, [&](const std::filesystem::path& templateName2) {
                    const auto shader = mManager.getShader(Files::pathToUnicodeString(templateName2), mDefines);
                    ASSERT_TRUE(shader);
                    const std::string expected
                        = "#version 120\n"
                          "#line 0 1\n"
                          "#line 0 2\n"
                          "void foo() {}\n"
                          "\n"
                          "#line 0 0\n"
                          "\n"
                          "void bar() { foo() }\n"
                          "\n"
                          "#line 1 0\n"
                          "\n"
                          "void main() { bar() }\n";
                    EXPECT_EQ(shader->getShaderSource(), expected);
                });
            });
        });
    }

    TEST_F(ShaderManagerTest, get_shader_should_replace_defines)
    {
        const std::string content
            = "#version 120\n"
              "#define FLAG @flag\n"
              "void main() {}\n";

        withShaderFile(content, [&](const std::filesystem::path& templateName) {
            mDefines["flag"] = "1";
            const auto shader = mManager.getShader(Files::pathToUnicodeString(templateName), mDefines);
            ASSERT_TRUE(shader);
            const std::string expected
                = "#version 120\n"
                  "#define FLAG 1\n"
                  "void main() {}\n";
            EXPECT_EQ(shader->getShaderSource(), expected);
        });
    }

    TEST_F(ShaderManagerTest, get_shader_should_expand_loop)
    {
        const std::string content
            = "#version 120\n"
              "@foreach index @list\n"
              "    varying vec4 foo@index;\n"
              "@endforeach\n"
              "void main() {}\n";

        withShaderFile(content, [&](const std::filesystem::path& templateName) {
            mDefines["list"] = "1,2,3";
            const auto shader = mManager.getShader(Files::pathToUnicodeString(templateName), mDefines);
            ASSERT_TRUE(shader);
            const std::string expected
                = "#version 120\n"
                  "    varying vec4 foo1;\n"
                  "    varying vec4 foo2;\n"
                  "    varying vec4 foo3;\n"
                  "\n"
                  "#line 5\n"
                  "void main() {}\n";
            EXPECT_EQ(shader->getShaderSource(), expected);
        });
    }

    TEST_F(ShaderManagerTest, get_shader_should_replace_loops_with_conditions)
    {
        const std::string content
            = "#version 120\n"
              "@foreach index @list\n"
              "    varying vec4 foo@index;\n"
              "@endforeach\n"
              "void main()\n"
              "{\n"
              "#ifdef BAR\n"
              "@foreach index @list\n"
              "    foo@index = vec4(1.0);\n"
              "@endforeach\n"
              "#elif BAZ\n"
              "@foreach index @list\n"
              "    foo@index = vec4(2.0);\n"
              "@endforeach\n"
              "#else\n"
              "@foreach index @list\n"
              "    foo@index = vec4(3.0);\n"
              "@endforeach\n"
              "#endif\n"
              "}\n";

        withShaderFile(content, [&](const std::filesystem::path& templateName) {
            mDefines["list"] = "1,2,3";
            const auto shader = mManager.getShader(Files::pathToUnicodeString(templateName), mDefines);
            ASSERT_TRUE(shader);
            const std::string expected
                = "#version 120\n"
                  "    varying vec4 foo1;\n"
                  "    varying vec4 foo2;\n"
                  "    varying vec4 foo3;\n"
                  "\n"
                  "#line 5\n"
                  "void main()\n"
                  "{\n"
                  "#ifdef BAR\n"
                  "    foo1 = vec4(1.0);\n"
                  "    foo2 = vec4(1.0);\n"
                  "    foo3 = vec4(1.0);\n"
                  "\n"
                  "#line 11\n"
                  "#elif BAZ\n"
                  "#line 12\n"
                  "    foo1 = vec4(2.0);\n"
                  "    foo2 = vec4(2.0);\n"
                  "    foo3 = vec4(2.0);\n"
                  "\n"
                  "#line 15\n"
                  "#else\n"
                  "#line 16\n"
                  "    foo1 = vec4(3.0);\n"
                  "    foo2 = vec4(3.0);\n"
                  "    foo3 = vec4(3.0);\n"
                  "\n"
                  "#line 19\n"
                  "#endif\n"
                  "#line 20\n"
                  "}\n";
            EXPECT_EQ(shader->getShaderSource(), expected);
        });
    }

    TEST_F(ShaderManagerTest, get_shader_should_fail_on_absent_template_parameters_in_single_line_comments)
    {
        const std::string content
            = "#version 120\n"
              "// #define FLAG @flag\n"
              "void main() {}\n";

        withShaderFile(content, [&](const std::filesystem::path& templateName) {
            EXPECT_FALSE(mManager.getShader(Files::pathToUnicodeString(templateName), mDefines));
        });
    }

    TEST_F(ShaderManagerTest, get_shader_should_fail_on_absent_template_parameter_in_multi_line_comments)
    {
        const std::string content
            = "#version 120\n"
              "/* #define FLAG @flag */\n"
              "void main() {}\n";

        withShaderFile(content, [&](const std::filesystem::path& templateName) {
            EXPECT_FALSE(mManager.getShader(Files::pathToUnicodeString(templateName), mDefines));
        });
    }
}

namespace
{
    TEST_F(ShaderManagerTest, NormalSpecularAndHeightPermutationsStayDistinctAcrossSiblings)
    {
        const auto directory = TestingOpenMW::outputFilePathWithSubDir("normal-material");
        std::filesystem::create_directories(directory / "compatibility");
        for (const auto extension : {".vert", ".frag"})
        {
            std::ofstream file(directory / "compatibility" / (std::string("material") + extension));
            file << "#version 120\nvoid main(){" << (std::string_view(extension) == ".vert" ? "gl_Position" : "gl_FragColor")
                 << "=vec4(@normalMap,@normalMapSpecular,@parallax,1);}";
        }
        mManager.setShaderPath(directory);
        VFS::Manager vfs;
        Resource::ImageManager images(&vfs, 0);
        ShaderVisitor visitor(mManager, images, "material");
        osg::ref_ptr<osg::Group> root = new osg::Group;
        // Each child replaces a parent height texture. Neither the specular
        // mask nor the ordinary normal map may inherit its parallax flag.
        root->getOrCreateStateSet()->setTextureAttribute(0, new osg::Texture2D, osg::StateAttribute::ON);
        root->getStateSet()->setTextureAttribute(0, new SceneUtil::TextureType("normalHeightMap"), osg::StateAttribute::ON);
        const std::array<std::pair<const char*, const char*>, 3> cases{{
            {"normalSpecularMap", "vec4(1,1,0,1)"},
            {"normalHeightMap", "vec4(1,0,1,1)"},
            {"normalMap", "vec4(1,0,0,1)"}}};
        for (const auto& [type, expected] : cases)
        {
            osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
            const osg::Vec3f vertices[] = {{0,0,0},{1,0,0},{0,1,0}};
            const osg::Vec3f normals[] = {{0,0,1},{0,0,1},{0,0,1}};
            const osg::Vec2f uv[] = {{0,0},{1,0},{0,1}};
            geometry->setVertexArray(new osg::Vec3Array(3, vertices));
            geometry->setNormalArray(new osg::Vec3Array(3, normals), osg::Array::BIND_PER_VERTEX);
            geometry->setTexCoordArray(0, new osg::Vec2Array(3, uv));
            geometry->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLES,0,3));
            auto* state = geometry->getOrCreateStateSet();
            state->setTextureAttribute(0, new osg::Texture2D, osg::StateAttribute::ON);
            state->setTextureAttribute(0, new SceneUtil::TextureType(type), osg::StateAttribute::ON);
            root->addChild(geometry);
        }
        root->accept(visitor);
        for (unsigned i = 0; i < cases.size(); ++i)
        {
            auto* state = root->getChild(i)->getStateSet(); ASSERT_NE(state, nullptr);
            auto* program = dynamic_cast<osg::Program*>(state->getAttribute(osg::StateAttribute::PROGRAM));
            ASSERT_NE(program, nullptr); ASSERT_GE(program->getNumShaders(), 2u);
            for (unsigned j = 0; j < program->getNumShaders(); ++j)
                EXPECT_NE(program->getShader(j)->getShaderSource().find(cases[i].second), std::string::npos);
            int unit = -1;
            ASSERT_NE(state->getUniform("normalMap"), nullptr);
            ASSERT_TRUE(state->getUniform("normalMap")->get(unit)); EXPECT_EQ(unit, 0);
        }
    }
}
