/*! This class performs pose-graph optimization. This class is based on the previous work of
 *  Miguel Algaba Borrego, in "http://code.google.com/p/kinect-rgbd-graphslam6d/".
 */

#include "GraphOptimizer_G2O.h"

using namespace g2o;
using namespace Eigen;

GraphOptimizer_G2O::GraphOptimizer_G2O()
{
    rigidTransformationType = GraphOptimizer::SixDegreesOfFreedom;

    optimizer.setVerbose(false);

    // variable-size block solver
    //    linearSolver = new LinearSolverCholmod<BlockSolverX::PoseMatrixType>();
    linearSolver = new LinearSolverDense<BlockSolverX::PoseMatrixType>();
    solver_ptr = new BlockSolverX(linearSolver);
    OptimizationAlgorithmLevenberg* solver = new OptimizationAlgorithmLevenberg(solver_ptr);

    optimizer.setAlgorithm(solver);

    //Set the vertex index to 0
    vertexIdx=0;
}

int GraphOptimizer_G2O::addVertex(Matrix4d& vertexPose)
{
//    if(rigidTransformationType==GraphOptimizer::SixDegreesOfFreedom) //Rigid transformation 6DoF
//    {
//        //Transform pose from Matrix4d into Isometry3d to set up rotation and translation for this node
//        Isometry3d cam; // camera pose
//        cam = Quaterniond(vertexPose.block(0,0,3,3));
//        cam.translation() = vertexPose.block(0,3,3,1);

//        // set up node
//        VertexSE3 *vc = new VertexSE3();
//        vc->setEstimate(cam);

//        vc->setId(vertex_id);      // vertex id

////        cerr << vertexPose.block(0,3,3,1).transpose() << " | " << Quaterniond(vertexPose.block(0,0,3,3)).transpose() << endl;

//        // set first cam pose fixed
//        if (vertex_id==0)
//          vc->setFixed(true);

//        // add to optimizer
//        optimizer.addVertex(vc);

//        vertex_id++;
//    }
//    else //Rigid transformation 3DoF
//    {
//        //Transform Matrix4d into 2D translation and rotation for g2o
//        SE2 pose(vertexPose(0,3),vertexPose(1,3),atan2(vertexPose(1,0),vertexPose(0,0)));

//        // set up node
//        VertexSE2 *vc = new VertexSE2();
//        vc->estimate() = pose;
//        vc->setId(vertexIdx);      // vertex id

//        // set first pose fixed
//        if (vertexIdx==0){
//            vc->setFixed(true);
//        }

//        // add to optimizer
//        optimizer.addVertex(vc);
//    }

    //Update vertex index
    vertexIdx++;

    //Return the added vertex index
    return vertexIdx-1;
}

void GraphOptimizer_G2O::addEdge(const int fromIdx,
                                 const int toIdx,
                                 Matrix4d& relativePose,
                                 Matrix<double,Dynamic,Dynamic>& informationMatrix)
{
//    if(rigidTransformationType==GraphOptimizer::SixDegreesOfFreedom) //Rigid transformation 6DoF
//    {
//        //Transform Matrix4d into 3D traslation and rotation for g2o
//        Vector3d t(relativePose(0,3),relativePose(1,3),relativePose(2,3));
//        Quaterniond q(relativePose.block(0,0,3,3));
//        SE3Quat transf(q,t);	// relative transformation

//        EdgeSE3* edge = new EdgeSE3;
//        edge->vertices()[0] = optimizer.vertex(fromIdx);
//        edge->vertices()[1] = optimizer.vertex(toIdx);
//        edge->setMeasurement(transf);

//        //Set the information matrix to identity
//        edge->setInformation(informationMatrix);

//        optimizer.addEdge(edge);
//    }
//    else //Rigid transformation 3DoF
//    {
//        //Transform Matrix4d into 2D translation and rotation for g2o
//        SE2 transf(relativePose(0,3),relativePose(1,3),atan2(relativePose(1,0),relativePose(0,0))); // relative transformation

//        EdgeSE2* edge = new EdgeSE2;
//        edge->vertices()[0] = optimizer.vertex(fromIdx);
//        edge->vertices()[1] = optimizer.vertex(toIdx);
//        edge->setMeasurement(transf);

//        //Set the information matrix to identity
//        edge->setInformation(informationMatrix);

//        optimizer.addEdge(edge);
//    }
}

void GraphOptimizer_G2O::optimizeGraph()
{
//    //Prepare and run the optimization
//    optimizer.initializeOptimization();

//    //Set the initial Levenberg-Marquardt lambda
////    optimizer.setUserLambdaInit(0.01);

//    optimizer.computeActiveErrors();
//    cout << "Initial chi2 = " << FIXED(optimizer.chi2()) << endl;

//    optimizer.setVerbose(true);

//    //Run optimization
//    optimizer.optimize(10);
}

void GraphOptimizer_G2O::getPoses(std::vector<Matrix4d, aligned_allocator<Matrix4d > >& poses)
{
//    poses.clear();
//    poses.resize(vertexIdx);

//    #pragma omp parallel for
//    for(int poseIdx=0;poseIdx<vertexIdx;poseIdx++)
//    {
//        Matrix4d optimizedPose = Matrix4d::Zero();

//        if(rigidTransformationType==GraphOptimizer::SixDegreesOfFreedom) //Rigid transformation 6DoF
//        {
//            //Transform the vertex pose from G2O quaternion and traslation vector to Matrix4d
//            VertexSE3* vertex = dynamic_cast<VertexSE3*>(optimizer.vertex(poseIdx));
//            double optimizedPoseQuaternion[7];
//            vertex->getEstimateData(optimizedPoseQuaternion);

//            double qx,qy,qz,qr,qx2,qy2,qz2,qr2;

//            Quaterniond quat;
//            quat.x()=optimizedPoseQuaternion[3];
//            quat.y()=optimizedPoseQuaternion[4];
//            quat.z()=optimizedPoseQuaternion[5];
//            quat.w()=optimizedPoseQuaternion[6];
//            Matrix3d rotationMatrix(quat);

//            for(int i=0;i<3;i++)
//            {
//                for(int j=0;j<3;j++)
//                {
//                    optimizedPose(i,j)=rotationMatrix(i,j);
//                }
//            }
//            optimizedPose(0,3)=optimizedPoseQuaternion[0];
//            optimizedPose(1,3)=optimizedPoseQuaternion[1];
//            optimizedPose(2,3)=optimizedPoseQuaternion[2];
//            optimizedPose(3,3)=1;
//        }
//        else //Rigid transformation 3DoF
//        {
//            //Transform the vertex pose from G2O SE2 pose to Matrix4d
//            VertexSE2* vertex = dynamic_cast<VertexSE2*>(optimizer.vertex(poseIdx));
//            double optimizedPoseSE2[3];
//            vertex->getEstimateData(optimizedPoseSE2);

//            optimizedPose(0,3)=optimizedPoseSE2[0];//x
//            optimizedPose(1,3)=optimizedPoseSE2[1];//y
//            optimizedPose(0,0)=cos(optimizedPoseSE2[2]);
//            optimizedPose(0,1)=-sin(optimizedPoseSE2[2]);
//            optimizedPose(1,0)=sin(optimizedPoseSE2[2]);
//            optimizedPose(1,1)=cos(optimizedPoseSE2[2]);
//            optimizedPose(2,2)=1;
//            optimizedPose(3,3)=1;
//        }

//        //Set the optimized pose to the vector of poses
//        poses[poseIdx]=optimizedPose;
//    }
}

void GraphOptimizer_G2O::saveGraph(std::string fileName)
{
        //Save the graph to file
        optimizer.save(fileName.c_str(),0);
}

