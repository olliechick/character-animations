//  ========================================================================
//  COSC422: Advanced Computer Graphics;  University of Canterbury (2019)
//
//  Assignment 2, part 2
//
//  By Ollie Chick, adapted from lab files
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

#define TAU 6.28318530718

//----------Globals----------------------------
const aiScene *scene = NULL;
const aiScene *scene2 = NULL;
aiVector3D scene_min, scene_max, scene_center;
std::map<int, int> texIdMap;

struct vec3 {
    float x;
    float y;
    float z;
};

vec3 eye, lookAt;
float camAngle = 0;
float camDistance = 0;

int tDuration; //Animation duration in ticks.
int currTick = 0; //current tick
float timeStep = 50; //Animation time step in milliseconds

struct meshInit {
    int mNumVertices;
    aiVector3D *mVertices;
    aiVector3D *mNormals;
};

meshInit *initData;

//------------Modify the following as needed----------------------
float materialCol[4] = {0.2, 0.2, 1, 1};   //Default material colour (not used if model's colour is available)
bool replaceCol = false;                       //Change to 'true' to set the model's colour to the above colour
float lightPosn[4] = {0, 50, 50, 1};         //Default light's position
bool twoSidedLight = false;                       //Change to 'true' to enable two-sided lighting


void loadAnimation(const char *fileName, int animationIndex)
{
    scene2 = aiImportFile(fileName, aiProcessPreset_TargetRealtime_MaxQuality);
    if (scene2 == NULL) exit(1);
    tDuration = scene2->mAnimations[0]->mDuration;
}

//-------Loads model data from fileName1, animation data from fileName2 and creates a scene object----------
bool loadModel(const char *fileName1, const char *fileName2)
{
    scene = aiImportFile(fileName1, aiProcessPreset_TargetRealtime_MaxQuality);
    if (scene == NULL) exit(1);
    //printSceneInfo(scene);
    //printMeshInfo(scene);
    //printTreeInfo(scene->mRootNode);
    //printBoneInfo(scene);
    //printAnimInfo(scene);  //WARNING:  This may generate a lengthy output if the model has animation data
    loadAnimation(fileName2, 0);

    // Store initial mesh data
    initData = new meshInit[scene->mNumMeshes];
    for (uint m = 0; m < scene->mNumMeshes; m++) {
        aiMesh *mesh = scene->mMeshes[m];
        uint numVert = mesh->mNumVertices;
        (initData + m)->mNumVertices = numVert;
        (initData + m)->mVertices = new aiVector3D[numVert];
        (initData + m)->mNormals = new aiVector3D[numVert];
        for (uint v = 0; v < numVert; v++) {
            (initData + m)->mVertices[v] = mesh->mVertices[v];
            (initData + m)->mNormals[v] = mesh->mNormals[v];
        }
    }

    return true;
}

//-------------Loads texture files using DevIL library-------------------------------
void loadGLTextures(const aiScene *scene)
{
    /* initialization of DevIL */
    ilInit();
    if (scene->HasTextures()) {
        std::cout << "Support for meshes with embedded textures is not implemented" << endl;
        return;
    }

    /* scan scene's materials for textures */
    /* Simplified version: Retrieves only the first texture with index 0 if present*/
    for (unsigned int m = 0; m < scene->mNumMaterials; ++m) {
        aiString path;  // filename

        if (scene->mMaterials[m]->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS) {
            char *textureFilename = strrchr(path.data, '/');
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
            if (ilLoadImage((ILstring) path.data))   //if success
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
            } else {
                cout << "Couldn't load Image: " << path.data << endl;
            }
        }
    }  //loop for material

}

// ------A recursive function to traverse scene graph and render each mesh----------
void render(const aiScene *sc, const aiNode *nd)
{
    aiMatrix4x4 m = nd->mTransformation;
    aiMesh *mesh;
    aiFace *face;
    aiMaterial *mtl;
    GLuint texId;
    aiColor4D diffuse;
    int meshIndex, materialIndex;

    aiTransposeMatrix4(&m);   //Convert to column-major order

    glMultMatrixf((float *) &m);   //Multiply by the transformation matrix for this node

    // Draw all meshes assigned to this node
    for (uint n = 0; n < nd->mNumMeshes; n++) {
        meshIndex = nd->mMeshes[n];          //Get the mesh indices from the current node
        mesh = scene->mMeshes[meshIndex];    //Using mesh index, get the mesh object

        materialIndex = mesh->mMaterialIndex;  //Get material index attached to the mesh
        mtl = sc->mMaterials[materialIndex];
        if (replaceCol)
            glColor4fv(materialCol);   //User-defined colour
        else if (AI_SUCCESS ==
                 aiGetMaterialColor(mtl, AI_MATKEY_COLOR_DIFFUSE, &diffuse))  //Get material colour from model
            glColor4f(diffuse.r, diffuse.g, diffuse.b, 1.0);
        else
            glColor4fv(materialCol);   //Default material colour

        if (mesh->HasTextureCoords(0)) {
            glBindTexture(GL_TEXTURE_2D, texIdMap[mesh->mMaterialIndex]);
            glEnable(GL_TEXTURE_2D);
        }


        //Get the polygons from each mesh and draw them
        for (uint k = 0; k < mesh->mNumFaces; k++) {
            face = &mesh->mFaces[k];
            GLenum face_mode;

            switch (face->mNumIndices) {
                case 1:
                    face_mode = GL_POINTS;
                    break;
                case 2:
                    face_mode = GL_LINES;
                    break;
                case 3:
                    face_mode = GL_TRIANGLES;
                    break;
                default:
                    face_mode = GL_POLYGON;
                    break;
            }

            glBegin(face_mode);

            for (uint i = 0; i < face->mNumIndices; i++) {
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
    for (uint i = 0; i < nd->mNumChildren; i++) {
        glPushMatrix();
        {
            render(sc, nd->mChildren[i]);
        }
        glPopMatrix();
    }
}

void drawFloor()
{
    int floorSize = 2000; // half of width/depth
    int increment = 50; // length of each square
    int floorHeight = 0;

    bool flag = false;

    glNormal3f(0.0, 1.0, 0.0);

    glBegin(GL_QUADS);
    for (int i = -floorSize; i <= floorSize; i += increment) {
        for (int j = -floorSize; j <= floorSize; j += increment) {
            if (flag) glColor3f(0.258824, 0.529412, 0.960784); // blue
            else glColor3f(0.258824, 0.960784, 0.690196); // green
            glVertex3f(i, floorHeight, j);
            glVertex3f(i, floorHeight, j + increment);
            glVertex3f(i + increment, floorHeight, j + increment);
            glVertex3f(i + increment, floorHeight, j);
            flag = !flag;
        }
    }
    glEnd();
}

//--------------------OpenGL initialization------------------------
void initialise()
{
    eye = {0, 0, 3};
    lookAt = {0, 0, 0};
    camDistance = 3;

    float ambient[4] = {0.2, 0.2, 0.2, 1.0};  //Ambient light
    float white[4] = {1, 1, 1, 1};            //Light's colour
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
    loadModel("Models/Mannequin/mannequin.fbx",
              "Models/Mannequin/run.fbx");            //<<<-------------Specify input file name here
    loadGLTextures(scene);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(35, 1, 1.0, 1000.0);
}

void updateNodeMatrices(int tick)
{
    int index;
    aiAnimation *anim = scene2->mAnimations[0];
    aiMatrix4x4 matPos, matRot, matProd;
    aiMatrix3x3 matRot3;
    aiNode *nd;
    for (uint i = 0; i < anim->mNumChannels; i++) {
        matPos = aiMatrix4x4(); //Identity
        matRot = aiMatrix4x4();
        aiNodeAnim *ndAnim = anim->mChannels[i]; //Channel
        if (ndAnim->mNumPositionKeys > 1) index = tick;
        else index = 0;
        aiVector3D posn = (ndAnim->mPositionKeys[index]).mValue;
        if (ndAnim->mNodeName != aiString("free3dmodel_skeleton"))
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

void transformVertices(int tick)
{
    aiNode *node, *parent;

    // transform all meshes assigned to this node
    for (uint m = 0; m < scene->mNumMeshes; m++) {
        aiMesh *mesh = scene->mMeshes[m];
        aiMatrix4x4 weightedMatrices[mesh->mNumVertices] = {aiMatrix4x4()};
        for (uint b = 0; b < mesh->mNumBones; b++) {
            aiBone *bone = mesh->mBones[b];
            aiMatrix4x4 offsetMatrix = bone->mOffsetMatrix; // get offset matrix of bone
            node = scene->mRootNode->FindNode(bone->mName); // find node corresponding to bone
            aiMatrix4x4 matrixProduct = node->mTransformation * offsetMatrix; // form the matrix product
            while ((parent = node->mParent) != NULL) {
                matrixProduct = parent->mTransformation * matrixProduct;
                node = parent;
            }

            for (uint k = 0; k < bone->mNumWeights; k++) {
                uint v = (bone->mWeights[k]).mVertexId;
                float weight = (bone->mWeights[k]).mWeight;
                aiMatrix4x4 weightMatrix = aiMatrix4x4(weight, 0, 0, 0, 0, weight, 0, 0, 0, 0, weight, 0, 0, 0, 0,
                                                       weight);
                if (weightedMatrices[v] == aiMatrix4x4()) {
                    weightedMatrices[v] = matrixProduct * weightMatrix;
                } else {
                    weightedMatrices[v] = weightedMatrices[v] + matrixProduct * weightMatrix;
                }
            }

        }
        for (uint b = 0; b < mesh->mNumBones; b++) {
            aiBone *bone = mesh->mBones[b];

            for (uint k = 0; k < bone->mNumWeights; k++) {
                uint v = (bone->mWeights[k]).mVertexId;
                aiMatrix4x4 normal = aiMatrix4x4(weightedMatrices[v]).Inverse().Transpose(); // form the normal matrix
                aiVector3D vert = (initData + m)->mVertices[v];
                aiVector3D norm = (initData + m)->mNormals[v];
                mesh->mVertices[v] = weightedMatrices[v] * vert;
                mesh->mNormals[v] = normal * norm;
            }
        }

    }
}

//----Timer callback----
void update(int value)
{
    if (currTick < tDuration) {
        updateNodeMatrices(currTick);
        transformVertices(currTick);
        glutTimerFunc(timeStep, update, 0);
        currTick++;
    } else {
        currTick = 0;
        glutTimerFunc(timeStep, update, 0);
    }
    if (currTick == 1) get_bounding_box(scene, &scene_min, &scene_max);
    glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y)
{
    if (key == 'r') {
        camAngle = 0;
        camDistance = 3;
    }
    glutPostRedisplay();
}

void special(int key, int x, int y)
{
    if (key == GLUT_KEY_UP) {
        camDistance--;
    } else if (key == GLUT_KEY_DOWN) {
        camDistance++;
    } else if (key == GLUT_KEY_PAGE_UP) {
        camDistance -= 10;
    } else if (key == GLUT_KEY_PAGE_DOWN) {
        camDistance += 10;
    } else if (key == GLUT_KEY_RIGHT) {
        camAngle += 0.01;
    } else if (key == GLUT_KEY_LEFT) {
        camAngle -= 0.01;
    } else if (key == GLUT_KEY_END) {
        camAngle += 0.1;
    } else if (key == GLUT_KEY_HOME) {
        camAngle -= 0.1;
    }

    // don't let it go closer than 3
    if (camDistance < 3) camDistance = 3;

    // keep cam angle between 0 and 2 pi
    if (camAngle > TAU) camAngle -= TAU;
    if (camAngle < TAU) camAngle += TAU;

    glutPostRedisplay();
}

//------The main display function---------
//----The model is first drawn using a display list so that all GL commands are
//    stored for subsequent display updates.
void display()
{
    eye = {lookAt.x + sin(camAngle) * camDistance, 0, lookAt.z + cos(camAngle) * camDistance};
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(eye.x, eye.y, eye.z, lookAt.x, lookAt.y, lookAt.z, 0, 1, 0);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosn);

    // scale the whole asset to fit into our view frustum
    float tmp = scene_max.x - scene_min.x;
    tmp = aisgl_max(scene_max.y - scene_min.y, tmp);
    tmp = aisgl_max(scene_max.z - scene_min.z, tmp);
    tmp = 1.f / tmp;
    glScalef(tmp, tmp, tmp);

    float xc = (scene_min.x + scene_max.x) * 0.5;
    float yc = (scene_min.y + scene_max.y) * 0.5;
    float zc = (scene_min.z + scene_max.z) * 0.5;
    // center the model
    glTranslatef(-xc, -yc, -zc);

    glPushMatrix();
    {
        render(scene, scene->mRootNode);
    }
    glPopMatrix();

    glPushMatrix();
    {
        glDisable(GL_TEXTURE_2D);
        drawFloor();
    }
    glPopMatrix();

    glutSwapBuffers();
}


int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(600, 600);
    glutCreateWindow("Ollie Chick COSC422 assignment 2 part 2");
    glutInitContextVersion(4, 2);
    glutInitContextProfile(GLUT_CORE_PROFILE);

    initialise();
    glutDisplayFunc(display);
    glutTimerFunc(timeStep, update, 0);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutMainLoop();

    aiReleaseImport(scene);
}

