//  ========================================================================
//  COSC422: Advanced Computer Graphics;  University of Canterbury (2019)
//
//  FILE NAME: ModelLoader.cpp
//  
//  Press key '1' to toggle 90 degs model rotation about x-axis on/off.
//  ========================================================================

#include <iostream>
#include <map>
#include <GL/freeglut.h>
#include <IL/il.h>
using namespace std;

#include <assimp/cimport.h>
#include <assimp/types.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "assimp_extras.h"

//----------Globals----------------------------
const aiScene* scene = NULL;
float angle = 0;
aiVector3D scene_min, scene_max, scene_center;
std::map<int, int> texIdMap;

int tDuration; //Animation duration in ticks.
int currTick = 0; //current tick
float timeStep = 50; //Animation time step = 50 m.sec

//------------Modify the following as needed----------------------
float materialCol[4] = { 0.2, 0.2, 1, 1 };   //Default material colour (not used if model's colour is available)
bool replaceCol = false;					   //Change to 'true' to set the model's colour to the above colour
float lightPosn[4] = { 0, 50, 50, 1 };         //Default light's position
bool twoSidedLight = false;					   //Change to 'true' to enable two-sided lighting

//-------Loads model data from file and creates a scene object----------
bool loadModel(const char* fileName)
{
	scene = aiImportFile(fileName, aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_Debone);
	if(scene == NULL) exit(1);
	//printSceneInfo(scene);
	//printMeshInfo(scene);
	//printTreeInfo(scene->mRootNode);
	//printBoneInfo(scene);
	//printAnimInfo(scene);  //WARNING:  This may generate a lengthy output if the model has animation data
	tDuration = scene->mAnimations[0]->mDuration;
	return true;
}

//-------------Loads texture files using DevIL library-------------------------------
void loadGLTextures(const aiScene* scene)
{
	/* initialization of DevIL */
	ilInit();
	if (scene->HasTextures())
	{
		std::cout << "Support for meshes with embedded textures is not implemented" << endl;
		return;
	}

	/* scan scene's materials for textures */
	/* Simplified version: Retrieves only the first texture with index 0 if present*/
	for (unsigned int m = 0; m < scene->mNumMaterials; ++m)
	{
		aiString path;  // filename

		if (scene->mMaterials[m]->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS)
		{
		    char* textureFilename = strrchr(path.data, '/');
		    strncpy(path.data, "Models/ArmyPilot", 24);
		    strncat(path.data, textureFilename, 1000);
			glEnable(GL_TEXTURE_2D);
			ILuint imageId;
			GLuint texId;
			ilGenImages(1, &imageId);
			glGenTextures(1, &texId);
			texIdMap[m] = texId;   //store tex ID against material id in a hash map
			ilBindImage(imageId); /* Binding of DevIL image name */
			ilEnable(IL_ORIGIN_SET);
			ilOriginFunc(IL_ORIGIN_LOWER_LEFT);
			if (ilLoadImage((ILstring)path.data))   //if success
			{
				/* Convert image to RGBA */
				ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);

				/* Create and load textures to OpenGL */
				glBindTexture(GL_TEXTURE_2D, texId);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ilGetInteger(IL_IMAGE_WIDTH),
					ilGetInteger(IL_IMAGE_HEIGHT), 0, GL_RGBA, GL_UNSIGNED_BYTE,
					ilGetData());
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				cout << "Texture:" << path.data << " successfully loaded." << endl;
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
				glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
				glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
			}
			else
			{
				cout << "Couldn't load Image: " << path.data << endl;
			}
		}
	}  //loop for material

}

void drawObject(const aiScene* sc, const aiNode* nd)
{
    aiMatrix4x4 m = nd->mTransformation;
    aiMesh* mesh;
    aiFace* face;
    aiMaterial* mtl;
    GLuint texId;
    aiColor4D diffuse;
    int meshIndex, materialIndex;

    aiTransposeMatrix4(&m);   //Convert to column-major order

    glMultMatrixf((float*)&m);   //Multiply by the transformation matrix for this node

    // Draw all meshes assigned to this node
    for (uint n = 0; n < nd->mNumMeshes; n++)
    {
        meshIndex = nd->mMeshes[n];          //Get the mesh indices from the current node
        mesh = scene->mMeshes[meshIndex];    //Using mesh index, get the mesh object

        materialIndex = mesh->mMaterialIndex;  //Get material index attached to the mesh
        mtl = sc->mMaterials[materialIndex];
        if (replaceCol)
            glColor4fv(materialCol);   //User-defined colour
        else if (AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_DIFFUSE, &diffuse))  //Get material colour from model
            glColor4f(diffuse.r, diffuse.g, diffuse.b, 1.0);
        else
            glColor4fv(materialCol);   //Default material colour

        if (mesh->HasTextureCoords(0)) {
            glBindTexture(GL_TEXTURE_2D, texIdMap[mesh->mMaterialIndex]);
            glEnable(GL_TEXTURE_2D);
        }


        //Get the polygons from each mesh and draw them
        for (uint k = 0; k < mesh->mNumFaces; k++)
        {
            face = &mesh->mFaces[k];
            GLenum face_mode;

            switch(face->mNumIndices)
            {
                case 1: face_mode = GL_POINTS; break;
                case 2: face_mode = GL_LINES; break;
                case 3: face_mode = GL_TRIANGLES; break;
                default: face_mode = GL_POLYGON; break;
            }

            glBegin(face_mode);

            for(uint i = 0; i < face->mNumIndices; i++) {
                uint vertexIndex = face->mIndices[i];

                if (mesh->HasVertexColors(0))
                    glColor4fv((GLfloat * ) & mesh->mColors[0][vertexIndex]);

                //Assign texture coordinates here
                if (mesh->HasTextureCoords(0))
                    glTexCoord2f(mesh->mTextureCoords[0][vertexIndex].x, mesh->mTextureCoords[0][vertexIndex].y);

                if (mesh->HasNormals())
                    glNormal3fv(&mesh->mNormals[vertexIndex].x);

                glVertex3fv(&mesh->mVertices[vertexIndex].x);
            }

            glEnd();
        }
    }

    // Draw all children
    for (uint i = 0; i < nd->mNumChildren; i++){
        glPushMatrix();
        drawObject(sc, nd->mChildren[i]);
        glPopMatrix();
    }
}

// ------A recursive function to traverse scene graph and render each mesh----------
void render (const aiScene* sc, const aiNode* nd)
{
    /*float gx = lightPosn[0];
    float gy = lightPosn[1];
    float gz = lightPosn[2];
    float shadowMat[16] = {gy,0,0,0, -gx,0,-gz,-1, 0,0,gy,0, 0,0,0,gy};

    glPushMatrix();
    glMultMatrixf(shadowMat);
    materialCol[2] = 0.2;
    replaceCol = true;
    drawObject(sc, nd);
    glPopMatrix();
*/
	glPushMatrix();
    materialCol[2] = 1.0;
    replaceCol = false;
    glRotatef(90, 1, 0, 0);
	drawObject(sc, nd);
	glPopMatrix();
}

void drawFloor()
{
    bool flag = false;

    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    for(int x = -5000; x <= 5000; x += 50)
    {
        for(int z = -5000; z <= 5000; z += 50)
        {
            if (flag) glColor3f(0.258824, 0.529412, 0.960784); // blue
            else glColor3f(0.258824, 0.960784, 0.690196); // green
            glVertex3f(x, 0, z);
            glVertex3f(x, 0, z+50);
            glVertex3f(x+50, 0, z+50);
            glVertex3f(x+50, 0, z);
            flag = !flag;
        }
    }
    glEnd();
}

//--------------------OpenGL initialization------------------------
void initialise()
{
	float ambient[4] = { 0.2, 0.2, 0.2, 1.0 };  //Ambient light
	float white[4] = { 1, 1, 1, 1 };			//Light's colour
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_NORMALIZE);
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, white);
	glLightfv(GL_LIGHT0, GL_SPECULAR, white);
	if (twoSidedLight) glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 1);

	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, white);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 50);
	glColor4fv(materialCol);
	loadModel("Models/ArmyPilot/ArmyPilot.x");			//<<<-------------Specify input file name here
	loadGLTextures(scene);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(35, 1, 1.0, 1000.0);
}

void updateNodeMatrices(int tick)
{
    int index;
    aiAnimation* anim = scene->mAnimations[0];
    aiMatrix4x4 matPos, matRot, matProd;
    aiMatrix3x3 matRot3;
    aiNode* nd;
    for (uint i = 0; i < anim->mNumChannels; i++)
    {
        matPos = aiMatrix4x4(); //Identity
        matRot = aiMatrix4x4();
        aiNodeAnim* ndAnim = anim->mChannels[i]; //Channel
        if (ndAnim->mNumPositionKeys > 1) index = tick;
        else index = 0;
        aiVector3D posn = (ndAnim->mPositionKeys[index]).mValue;
        matPos.Translation(posn, matPos);
        if (ndAnim->mNumRotationKeys > 1) index = tick;
        else index = 0;
        aiQuaternion rotn = (ndAnim->mRotationKeys[index]).mValue;
        matRot3 = rotn.GetMatrix();
        matRot = aiMatrix4x4(matRot3);
        matProd = matPos * matRot;
        nd = scene->mRootNode->FindNode(ndAnim->mNodeName);
        nd->mTransformation = matProd;
    }
}

//----Timer callback for continuous rotation of the model about y-axis----
void update(int value)
{
	if (currTick < tDuration) {
	    updateNodeMatrices(currTick);
	    glutTimerFunc(timeStep, update, 0);
	    currTick++;
	}
    /*if (currTick == 1) */get_bounding_box(scene, &scene_min, &scene_max);
	glutPostRedisplay();
}

//----Keyboard callback to toggle initial model orientation---
void keyboard(unsigned char key, int x, int y)
{
	glutPostRedisplay();
}

//------The main display function---------
//----The model is first drawn using a display list so that all GL commands are
//    stored for subsequent display updates.
void display()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(0, 0, 3, 0, 0, -5, 0, 1, 0);
	glLightfv(GL_LIGHT0, GL_POSITION, lightPosn);

	glRotatef(angle, 0.f, 1.f ,0.f);  //Continuous rotation about the y-axis

	// scale the whole asset to fit into our view frustum 
	float tmp = scene_max.x - scene_min.x;
	tmp = aisgl_max(scene_max.y - scene_min.y,tmp);
	tmp = aisgl_max(scene_max.z - scene_min.z,tmp);
	tmp = 1.f / tmp;
	glScalef(tmp, tmp, tmp);

	float xc = (scene_min.x + scene_max.x)*0.5;
	float yc = (scene_min.y + scene_max.y)*0.5;
	float zc = (scene_min.z + scene_max.z)*0.5;
	// center the model
	glTranslatef(-xc, -yc, -zc);

    render(scene, scene->mRootNode);

    glDisable(GL_TEXTURE_2D);
    drawFloor();

	glutSwapBuffers();
}



int main(int argc, char** argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(600, 600);
	glutCreateWindow("Model Loader");
	glutInitContextVersion (4, 2);
	glutInitContextProfile ( GLUT_CORE_PROFILE );

	initialise();
	glutDisplayFunc(display);
	glutTimerFunc(timeStep, update, 0);
	glutKeyboardFunc(keyboard);
	glutMainLoop();

	aiReleaseImport(scene);
}

