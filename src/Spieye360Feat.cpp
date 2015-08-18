/*
 *  Copyright (c) 2013, Universidad de Málaga - Grupo MAPIR and
 *                      INRIA Sophia Antipolis - LAGADIC Team
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *      * Neither the name of the holder(s) nor the
 *        names of its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Author: Eduardo Fernandez-Moral
 */

#include <Spieye360.h>

#ifdef _ENABLE_OPENMP
    #include <omp.h>
#endif

using namespace std;

/*! Constructor for the sensor RGBD360 (NUM_ASUS_SENSORS Asus XPL)*/
Spieye360::Spieye360(Calib360 *calib360) :
    calib(calib360)
{
}

/*! Load a spherical RGB-D image from the raw data stored in a binary file */
void Spieye360::loadFrame(string &binaryFile)
{
#if _PRINT_PROFILING
    double time_start = pcl::getTime();
#endif

    //    cout << "Opening binary file... " << binaryFile << endl;
    ifstream ifs(binaryFile.c_str(), ios::in | ios::binary);
    //    cout << " binary file... " << binaryFile << endl;

    try{// use scope to ensure archive goes out of scope before stream

        boost::archive::binary_iarchive binary360(ifs);

        cv::Mat timeStampMatrix;
        for(unsigned sensor_id=0; sensor_id < NUM_ASUS_SENSORS; ++sensor_id)
            binary360 >> frameRGBD_[sensor_id].getRGBImage() >> frameRGBD_[sensor_id].getDepthImage();
        binary360 >> timeStampMatrix;
        get_uint64_t_ofMatrixRepresentation(timeStampMatrix, timeStamp);

    }catch(const boost::archive::archive_exception &e){}

    //Close the binary bile
    ifs.close();

#if _ENABLE_OPENMP
    #pragma omp parallel num_threads(NUM_ASUS_SENSORS)
    {
        int sensor_id = omp_get_thread_num();
#else
    for(int sensor_id = 0; sensor_id < NUM_ASUS_SENSORS; sensor_id++)
    {
#endif
        frameRGBD_[sensor_id].loadDepthEigen();
    }

    //    bSphereCloudBuilt = false; // The spherical PointCloud of the frame just loaded is not built yet

#if _PRINT_PROFILING
    double time_end = pcl::getTime();
    cout << "Load Spieye360 took " << double (time_end - time_start) << endl;
#endif
}

/*! Undistort the omnidirectional depth image using the models acquired with CLAMS */
void Spieye360::undistort()
{
    double time_start = pcl::getTime();
    //    cout << "Undistort Spieye360\n";

#pragma omp parallel num_threads(NUM_ASUS_SENSORS)
    {
        int sensor_id = omp_get_thread_num();
        undistortDepthSensor(sensor_id);
        //      cv::eigen2cv(frameRGBD_[sensor_id].m_depthEigUndistort, frameRGBD_[sensor_id].getDepthImage());
    }

    //#if _PRINT_PROFILING
    double time_end = pcl::getTime();
    cout << "Undistort Spieye360 took " << double (time_end - time_start) << endl;
    //#endif

}

/*! Save the PbMap from an omnidirectional RGB-D image */
void Spieye360::savePlanes(string pathPbMap)
{
    mrpt::utils::CFileGZOutputStream serialize_planesLabeled(pathPbMap);
    serialize_planesLabeled << planes;
    serialize_planesLabeled.close();
}

/*! Save the pointCloud and PbMap from an omnidirectional RGB-D image */
void Spieye360::save(string &path, unsigned &frame)
{
    assert(!sphereCloud->empty() && planes.vPlanes.size() > 0);

    string cloudPath = path + mrpt::format("/sphereCloud_%d.pcd",frame);
    pcl::io::savePCDFile(cloudPath, *sphereCloud);

    string pbmapPath = path + mrpt::format("/spherePlanes_%d.pbmap",frame);
    savePlanes(pbmapPath);
}

/*! Serialize the omnidirectional RGB-D image */
void Spieye360::serialize(string &fileName)
{
    ofstream ofs_images(fileName.c_str(), ios::out | ios::binary);
    boost::archive::binary_oarchive oa_images(ofs_images);

    for(int sensor_id = 0; sensor_id < NUM_ASUS_SENSORS; sensor_id++)
        oa_images << frameRGBD_[sensor_id].getRGBImage() << frameRGBD_[sensor_id].getDepthImage();
    cv::Mat timeStampMatrix;
    getMatrixNumberRepresentationOf_uint64_t(timeStamp,timeStampMatrix);
    oa_images << timeStampMatrix;

    ofs_images.close();
}

/*! Concatenate the different RGB-D images to obtain the omnidirectional one (stich images without using the spherical representation) */
void Spieye360::fastStitchImage360() // Parallelized with OpenMP
{
#if _PRINT_PROFILING
    double time_start = pcl::getTime();
#endif

    int width_im = frameRGBD_[0].getRGBImage().rows;
    int width_SphereImg = frameRGBD_[0].getRGBImage().rows * NUM_ASUS_SENSORS;
    int height_SphereImg = frameRGBD_[0].getRGBImage().cols;
    sphereRGB = cv::Mat::zeros(height_SphereImg, width_SphereImg, CV_8UC3);
    sphereDepth = cv::Mat::zeros(height_SphereImg, width_SphereImg, CV_32FC1);

    for(int sensor_id = 0; sensor_id < NUM_ASUS_SENSORS; sensor_id++)
        //    #pragma omp parallel num_threads(NUM_ASUS_SENSORS)
    {
        //      int sensor_id = omp_get_thread_num();

        // RGB
        cv::Mat rgb_transposed, rgb_rotated;
        cv::transpose(frameRGBD_[NUM_ASUS_SENSORS-1-sensor_id].getRGBImage(), rgb_transposed);
        cv::flip(rgb_transposed, rgb_rotated, 0);
        cv::Mat tmp = sphereRGB(cv::Rect(sensor_id*width_im, 0, frameRGBD_[0].getRGBImage().rows, frameRGBD_[0].getRGBImage().cols));
        rgb_rotated.copyTo(tmp);

        // Depth
        //      cv::Mat depth_transposed, depth_rotated;
        //      cvTranspose(frameRGBD_[NUM_ASUS_SENSORS-1-sensor_id].getDepthImage(), depth_transposed);
        //      cv::flip(depth_transposed, depth_rotated, 0);
        //      cv::Mat tmp_depth =  sphereDepth(cv::Rect(sensor_id*width_im, 0, frameRGBD_[0].getDepthImage().rows, frameRGBD_[0].getDepthImage().cols));
        //      depth_rotated.copyTo(tmp_depth);
    }

#if _PRINT_PROFILING
    double time_end = pcl::getTime();
    cout << "fastStitchImage360 took " << double (time_end - time_start) << endl;
#endif

}

/*! Stitch RGB-D image using a spherical representation. The sensor 4 is looking forward in our robot platform, thus it is set in the center of the image */
void Spieye360::stitchSphericalImage(const int depth_type) // Parallelized with OpenMP
{
    //        cout << "stitchSphericalImage\n";
    double time_start = pcl::getTime();

    int width_SphereImg = frameRGBD_[0].getRGBImage().rows * NUM_ASUS_SENSORS;
    int height_SphereImg = width_SphereImg * 0.5 * 60.0/180; // Store only the part of the sphere which contains information
    sphereRGB = cv::Mat::zeros(height_SphereImg, width_SphereImg, CV_8UC3);
    sphereDepth = cv::Mat::zeros(height_SphereImg, width_SphereImg, CV_16UC1);
#pragma omp parallel num_threads(NUM_ASUS_SENSORS)
    //        for(unsigned sensor_id=0; sensor_id < NUM_ASUS_SENSORS; ++sensor_id)
    {
        int sensor_id = omp_get_thread_num();
        stitchImage(sensor_id);
        // cout << "Stitcher omp " << sensor_id << endl;
    }

    if(depth_type == CV_32FC1) // If the image is in milimetres, convert it to meters
        sphereDepth.convertTo(sphereDepth, CV_32FC1, 0.001 );

    double time_end = pcl::getTime();
    cout << "stitchSphericalImage took " << double (time_end - time_start)*1000. << " ms. \n";
}

/*! Downsample and filter the individual point clouds from the different Asus XPL */
void Spieye360::buildCloudsDownsampleAndFilter()
{
    double time_start = pcl::getTime();

#pragma omp parallel num_threads(NUM_ASUS_SENSORS)
    {
        int sensor_id = omp_get_thread_num();

        // Filter pointClouds
        pcl::FastBilateralFilter<pcl::PointXYZRGBA> filter;
        filter.setSigmaS(10.0);
        filter.setSigmaR(0.05);

#if DOWNSAMPLE_160
        //        DownsampleRGBD downsampler(2);
        //        frameRGBD_[sensor_id].setPointCloud(downsampler.downsamplePointCloud(frameRGBD_[sensor_id].getPointCloudUndist()));
        frameRGBD_[sensor_id].getDownsampledPointCloudUndist(2);
#endif

#if USE_BILATERAL_FILTER
        filter.setInputCloud(frameRGBD_[sensor_id].getPointCloud());
        filter.filter(*frameRGBD_[sensor_id].getPointCloud());
#endif
    }

    double time_end = pcl::getTime();
    cout << "Build single clouds + downsample + filter took " << double (time_end - time_start) << endl;
}

/*! Build the cloud from the 'sensor_id' Asus XPL */
void Spieye360::buildCloud_id(int sensor_id)
{
    pcl::FastBilateralFilter<pcl::PointXYZRGBA> filter;
    filter.setSigmaS (10.0);
    filter.setSigmaR (0.05);
    //  cout << "buildCloud_id1 " << sensor_id << "\n";

    // #pragma openmp
#if DOWNSAMPLE_160
    //      DownsampleRGBD downsampler(2);
    ////      cout << "buildCloud_id1\n";
    //      frameRGBD_[sensor_id].setPointCloud(downsampler.downsamplePointCloud(frameRGBD_[sensor_id].getPointCloudUndist()));
    ////    cout << "buildCloud_id2\n";
    frameRGBD_[sensor_id].getDownsampledPointCloudUndist(2);
#endif

#if USE_BILATERAL_FILTER
    filter.setInputCloud(frameRGBD_[sensor_id].getDownsampledPointCloudUndist(2));
    filter.filter(*frameRGBD_[sensor_id].getDownsampledPointCloudUndist(2));
#endif

    pcl::transformPointCloud(*frameRGBD_[sensor_id].getPointCloud(),*cloud_[sensor_id],calib->getRt_id(sensor_id));

    //    buildCloud_im_[sensor_id] = true;

    //  cout << "buildCloud_id3 " << sensor_id << "\n";
}

/*! Build the spherical point cloud by superimposing the NUM_ASUS_SENSORS point clouds from the NUM_ASUS_SENSORS Asus XPL
 * Z -> Forward, X -> Upwards, X -> Rightwards
 */
void Spieye360::buildPointCloud_rgbd360()
{
    //    if(bSphereCloudBuilt) // Avoid building twice the spherical point cloud
    //      return;

#if _PRINT_PROFILING
    double time_start = pcl::getTime();
#endif

    //      #pragma omp parallel num_threads(NUM_ASUS_SENSORS) // I still don't understand why this doesn't work
    for(unsigned sensor_id=0; sensor_id < NUM_ASUS_SENSORS; ++sensor_id)
    {
        //        int sensor_id = omp_get_thread_num();
        //            cout << "build " << sensor_id << endl;
#if USE_INTRINSIC_CALIB
    #if DOWNSAMPLE_160
        DownsampleRGBD downsampler(2);
        frameRGBD_[sensor_id].setPointCloud(downsampler.downsamplePointCloud(frameRGBD_[sensor_id].getPointCloudUndist()));
        //          frameRGBD_[sensor_id].getDownsampledPointCloudUndist(2);
    #else
        frameRGBD_[sensor_id].getPointCloudUndist();
    #endif
#else
    frameRGBD_[sensor_id].getPointCloud();
#endif
    }
    //      cout << "Downsample image\n";

#pragma omp parallel num_threads(NUM_ASUS_SENSORS)
    {
        int sensor_id = omp_get_thread_num();

#if USE_BILATERAL_FILTER
        pcl::FastBilateralFilter<pcl::PointXYZRGBA> filter;
        filter.setSigmaS (10.0);
        filter.setSigmaR (0.05);
        filter.setInputCloud(frameRGBD_[sensor_id].getPointCloud());
        filter.filter(*frameRGBD_[sensor_id].getPointCloud());
#endif

        //pcl::transformPointCloud(*frameRGBD_[sensor_id].getPointCloud(),*cloud_[sensor_id],calib->getRt_id(sensor_id));
//        float angle_offset_90 = -90; // X -> right
//        Eigen::Matrix4f rot_offset_90 = Eigen::Matrix4f::Identity(); rot_offset_90(0,0) = rot_offset_90(1,1) = cos(angle_offset_90*PI/180); rot_offset_90(0,1) = -sin(angle_offset_90*PI/180); rot_offset_90(1,0) = -rot_offset_90(0,1);
//        float angle_offset = 45; //0; //-22.5; //45;
//        Eigen::Matrix4f rot_offset = Eigen::Matrix4f::Identity(); rot_offset(1,1) = rot_offset(2,2) = cos(angle_offset*PI/180); rot_offset(1,2) = -sin(angle_offset*PI/180); rot_offset(2,1) = -rot_offset(1,2);
//        rot_offset = angle_offset_90 * rot_offset;
//        Eigen::Matrix4f Rt = rot_offset * calib->getRt_id(sensor_id);
//        cout << "Transforms \n" << calib->getRt_id(sensor_id) << "\n\n"
//                                    << rot_offset * calib->getRt_id(sensor_id) << "\n\n"
//                                    << calib->getRt_id(sensor_id) * rot_offset << "\n\n"
//                                    << rot_offset.inverse() * calib->getRt_id(sensor_id) << "\n\n"
//                                    << rot_offset * calib->getRt_id(sensor_id) * rot_offset.inverse() << "\n\n"
//                                    << rot_offset.inverse() * calib->getRt_id(sensor_id) * rot_offset << "\n\n";
//        pcl::transformPointCloud( *frameRGBD_[sensor_id].getPointCloud(), *cloud_[sensor_id], Rt );
        pcl::transformPointCloud( *frameRGBD_[sensor_id].getPointCloud(), *cloud_[sensor_id], calib->getRt_id(sensor_id) );
    }

    *sphereCloud = *cloud_[0];
    for(unsigned sensor_id=1; sensor_id < NUM_ASUS_SENSORS; ++sensor_id)
        *sphereCloud += *cloud_[sensor_id];

    sphereCloud->height = cloud_[0]->width;
    sphereCloud->width = NUM_ASUS_SENSORS * cloud_[0]->height;
    sphereCloud->is_dense = false;

    //    bSphereCloudBuilt = true;

#if _PRINT_PROFILING
    double time_end = pcl::getTime();
    cout << "PointCloud sphere construction took " << double (time_end - time_start) << endl;
#endif

}

/*! Fast version of the method 'buildPointCloud'. This one performs more poorly for plane segmentation. */
void Spieye360::buildPointCloud_fast()
{
#if _PRINT_PROFILING
    double time_start = pcl::getTime();
#endif

#pragma omp parallel num_threads(NUM_ASUS_SENSORS) // I still don't understand why this doesn't work
    //      for(unsigned sensor_id=0; sensor_id < NUM_ASUS_SENSORS; ++sensor_id)
    {
        int sensor_id = omp_get_thread_num();
#if DOWNSAMPLE_160
        frameRGBD_[sensor_id].getDownsampledPointCloudUndist(2);
#else
        frameRGBD_[sensor_id].getPointCloudUndist();
#endif

        pcl::transformPointCloud(*frameRGBD_[sensor_id].getPointCloud(),*cloud_[sensor_id],calib->getRt_id(sensor_id));
    }

    *sphereCloud = *cloud_[0];
    for(unsigned sensor_id=1; sensor_id < NUM_ASUS_SENSORS; ++sensor_id)
        *sphereCloud += *cloud_[sensor_id];

    sphereCloud->height = cloud_[0]->width;
    sphereCloud->width = NUM_ASUS_SENSORS * cloud_[0]->height;
    sphereCloud->is_dense = false;

#if _PRINT_PROFILING
    double time_end = pcl::getTime();
    cout << "buildPointCloud_fast took " << double (time_end - time_start) << endl;
#endif

}

/*! Build the spherical point cloud. The reference system is the one used by the INRIA SphericalStereo sensor. Z points forward, X points to the right and Y points downwards */
void Spieye360::buildPointCloud()
{
//    if(bSphereCloudBuilt) // Avoid building twice the spherical point cloud
//      return;

#if _PRINT_PROFILING
    cout << " Spieye360_stereo::buildPointCloud... " << endl;
    double time_start = pcl::getTime();
#endif

    sphereCloud->resize(sphereRGB.rows*sphereRGB.cols);
    sphereCloud->height = sphereRGB.rows;
    sphereCloud->width = sphereRGB.cols;
    sphereCloud->is_dense = false;
    const float min_depth = 0.4f;
    const float max_depth = 20.f;

    const float pixel_angle_ = 2*PI / sphereDepth.cols;
    const int half_width = sphereDepth.cols/2;

    //  Efficiency: store the values of the trigonometric functions
    Eigen::VectorXf v_sinTheta(sphereDepth.cols);
    Eigen::VectorXf v_cosTheta(sphereDepth.cols);
    float *sinTheta = &v_sinTheta[0];
    float *cosTheta = &v_cosTheta[0];
    for(int col_theta=-half_width; col_theta < half_width; ++col_theta)
    {
        float theta = (col_theta+0.5f)*pixel_angle_;
        *(sinTheta++) = sin(theta);
        *(cosTheta++) = cos(theta);
    }
    size_t start_row = (sphereRGB.cols-sphereRGB.rows) / 2;

    float *depth = sphereDepth.ptr<float>(0);
    cv::Vec3b *intensity = sphereRGB.ptr<cv::Vec3b>(0);
#if _ENABLE_OPENMP
    #pragma omp parallel for
#endif
    //for(int row_phi=0; row_phi < sphereDepth.rows; row_phi++)
    for(int row_phi=0, pixel_index=0; row_phi < sphereDepth.rows; row_phi++)//, row_phi += width_SphereImg)
    {
        float sin_phi = v_sinTheta[start_row+row_phi];
        float cos_phi = v_cosTheta[start_row+row_phi];

        int col_count = 0;
        for(int col_theta=-half_width; col_theta < half_width; ++col_theta, ++pixel_index, ++col_count)
        {
            if((*depth) > min_depth && (*depth) < max_depth)
            {
                //cout << min_depth << " depth " << *depth << " max_depth " << max_depth << endl;
                sphereCloud->points[pixel_index].x = (*depth) * cos_phi * v_sinTheta[col_count];
                sphereCloud->points[pixel_index].y = (*depth) * sin_phi;
                sphereCloud->points[pixel_index].z = (*depth) * cos_phi * v_cosTheta[col_count];
                sphereCloud->points[pixel_index].r = (*intensity)[2];
                sphereCloud->points[pixel_index].g = (*intensity)[1];
                sphereCloud->points[pixel_index].b = (*intensity)[0];
            }
            else
            {
                sphereCloud->points[pixel_index].x = numeric_limits<float>::quiet_NaN ();
                sphereCloud->points[pixel_index].y = numeric_limits<float>::quiet_NaN ();
                sphereCloud->points[pixel_index].z = numeric_limits<float>::quiet_NaN ();
            }
            ++depth;
            ++intensity;
        }
    }

#if _PRINT_PROFILING
    double time_end = pcl::getTime();
    cout << "Spieye360::buildPointCloud() took " << double (time_end - time_start)*1000 << " ms. \n";
#endif
}

///*! Build the spherical point cloud. The reference system is the one used by the INRIA SphericalStereo sensor. Z points forward, X points to the right and Y points downwards */
//void Spieye360::buildPointCloud2()
//{
////    if(bSphereCloudBuilt) // Avoid building twice the spherical point cloud
////      return;

//#if _PRINT_PROFILING
//    cout << " Spieye360_stereo::buildPointCloud2... " << endl;
//    double time_start = pcl::getTime();
//#endif

//    sphereCloud->resize(sphereRGB.rows*sphereRGB.cols);
//    sphereCloud->height = sphereRGB.rows;
//    sphereCloud->width = sphereRGB.cols;
//    sphereCloud->is_dense = false;
//    size_t img_size = sphereRGB.rows * sphereRGB.cols;

//    SphericalModel proj;
//    Eigen::MatrixXf xyz;
//    Eigen::VectorXi validPixels;
//    proj.reconstruct3D(sphereDepth, xyz, validPixels);

//    //float *depth = sphereDepth.ptr<float>(0);
//    cv::Vec3b *rgb = sphereRGB.ptr<cv::Vec3b>(0);

//#if _ENABLE_OPENMP
//    #pragma omp parallel for
//#endif
//    for(size_t i=0; i < img_size; i++)//, row_phi += width_SphereImg)
//    {
//        //if(validPixels(i) >= 0)
//        if(validPixels(i) != -1)
//        {
//            //cout << min_depth << " depth " << *depth << " max_depth " << max_depth << endl;
//            sphereCloud->points[i].x = xyz(i,0);
//            sphereCloud->points[i].y = xyz(i,1);
//            sphereCloud->points[i].z = xyz(i,2);
//            sphereCloud->points[i].r = (*rgb)[2];
//            sphereCloud->points[i].g = (*rgb)[1];
//            sphereCloud->points[i].b = (*rgb)[0];
//        }
//        else
//        {
//            sphereCloud->points[i].x = numeric_limits<float>::quiet_NaN ();
//            sphereCloud->points[i].y = numeric_limits<float>::quiet_NaN ();
//            sphereCloud->points[i].z = numeric_limits<float>::quiet_NaN ();
//        }
//        //++depth;
//        ++rgb;
//    }

//#if _PRINT_PROFILING
//    double time_end = pcl::getTime();
//    cout << "Spieye360::buildPointCloud2() took " << double (time_end - time_start)*1000 << " ms. \n";
//#endif
//}

/*! Create the PbMap of the spherical point cloud */
void Spieye360::segmentPlanes()
{
    cout << "Spieye360.segmentPlanes()\n";

#if _PRINT_PROFILING
    double time_start = pcl::getTime();
#endif

#if _ENABLE_OPENMP
    #pragma omp parallel num_threads(NUM_ASUS_SENSORS)
    {
        int sensor_id = omp_get_thread_num();
#else
    for(int sensor_id = 0; sensor_id < NUM_ASUS_SENSORS; sensor_id++)
    {
#endif
        segmentPlanesSensor(sensor_id);
    }

#if _PRINT_PROFILING
    double segmentation_end = pcl::getTime();
    cout << "Segmentation took " << double (segmentation_end - time_start)*1000 << " ms.\n";
#endif

    // Merge the big planes
    groupPlanes(); // Merge planes detected from adjacent sensors, and place them in "planes"
    mergePlanes(); // Merge big planes

#if _PRINT_PROFILING
    double extractPlanes_end = pcl::getTime();
    cout << planes.vPlanes.size() << " planes. Extraction took " << double (extractPlanes_end - time_start)*1000 << " ms.\n";
#endif

}

/*! Segment planes in each of the separate point clouds correspondint to each Asus XPL */
void Spieye360::segmentPlanesLocal()
{
    //double time_start = pcl::getTime();
    cout << "segmentPlanesLocal() " << NUM_ASUS_SENSORS << endl;

    //    #pragma omp parallel num_threads(NUM_ASUS_SENSORS)
    for(unsigned sensor_id=0; sensor_id < NUM_ASUS_SENSORS; sensor_id++)
    {
        //      int sensor_id = omp_get_thread_num();
        segmentPlanesLocalCam(sensor_id);
    }

    //    double time_end = pcl::getTime();
    //    cout << "Local-Plane extraction took " << double (time_end - time_start) << endl;
}

/*! Merge the planar patches that correspond to the same surface in the sphere */
void Spieye360::mergePlanes()
{
#if _PRINT_PROFILING
    double time_start = pcl::getTime();
#endif

    // Merge repeated planes
    for(size_t j = 0; j < planes.vPlanes.size(); j++) // numPrevPlanes
        if(planes.vPlanes[j].curvature < max_curvature_plane)
            for(size_t k = j+1; k < planes.vPlanes.size(); k++) // numPrevPlanes
                if(planes.vPlanes[k].curvature < max_curvature_plane)
                {
                    bool bSamePlane = false;
                    //        Eigen::Vector3f center_diff = planes.vPlanes[k].v3center - planes.vPlanes[j].v3center;
                    Eigen::Vector3f close_points_diff;
                    //float prev_dist = 1;
                    if( planes.vPlanes[j].v3normal.dot(planes.vPlanes[k].v3normal) > 0.99 )
                        if( fabs(planes.vPlanes[j].d - planes.vPlanes[k].d) < 0.45 )
                            //          if( BhattacharyyaDist_(plane1.hist_H, plane2.hist_H) > configLocaliser.hue_threshold )
                            //          if( fabs(planes.vPlanes[j].v3normal.dot(center_diff)) < max(0.07, 0.03*center_diff.norm() ) )
                        {
                            // Checking distances:
                            // a) Between an vertex and a vertex
                            // b) Between an edge and a vertex
                            // c) Between two edges (imagine two polygons on perpendicular planes)
                            for(unsigned i=1; i < planes.vPlanes[j].polygonContourPtr->size() && !bSamePlane; i++)
                                for(unsigned ii=1; ii < planes.vPlanes[k].polygonContourPtr->size(); ii++)
                                {
                                    close_points_diff = mrpt::pbmap::diffPoints(planes.vPlanes[j].polygonContourPtr->points[i], planes.vPlanes[k].polygonContourPtr->points[ii]);
                                    float dist = close_points_diff.norm();
                                    //                if( dist < prev_dist )
                                    //                  prev_dist = dist;
                                    if( dist < 0.3 && fabs(planes.vPlanes[j].v3normal.dot(close_points_diff)) < 0.06)
                                    {
                                        bSamePlane = true;
                                        break;
                                    }
                                }
                            // a) & b)
                            if(!bSamePlane)
                                for(unsigned i=1; i < planes.vPlanes[j].polygonContourPtr->size() && !bSamePlane; i++)
                                    for(unsigned ii=1; ii < planes.vPlanes[k].polygonContourPtr->size(); ii++)
                                    {
                                        float dist = sqrt(mrpt::pbmap::dist3D_Segment_to_Segment2(mrpt::pbmap::Segment(planes.vPlanes[j].polygonContourPtr->points[i],planes.vPlanes[j].polygonContourPtr->points[i-1]), mrpt::pbmap::Segment(planes.vPlanes[k].polygonContourPtr->points[ii],planes.vPlanes[k].polygonContourPtr->points[ii-1])));
                                        //                if( dist < prev_dist )
                                        //                  prev_dist = dist;
                                        if( dist < 0.3)
                                        {
                                            close_points_diff = mrpt::pbmap::diffPoints(planes.vPlanes[j].polygonContourPtr->points[i], planes.vPlanes[k].polygonContourPtr->points[ii]);
                                            if(fabs(planes.vPlanes[j].v3normal.dot(close_points_diff)) < 0.06)
                                            {
                                                bSamePlane = true;
                                                break;
                                            }
                                        }
                                    }
                        }

                    if( bSamePlane ) // The planes are merged if they are the same
                    {
                        // Update normal and center
                        assert(planes.vPlanes[j].inliers.size() > 0 &&  planes.vPlanes[k].inliers.size() > 0);
                        planes.vPlanes[j].mergePlane2(planes.vPlanes[k]);

                        // Update plane index
                        for(size_t h = k+1; h < planes.vPlanes.size(); h++)
                            --planes.vPlanes[h].id;

                        // Delete plane to merge
                        vector<mrpt::pbmap::Plane>::iterator itPlane = planes.vPlanes.begin();
                        for(size_t i = 0; i < k; i++)
                            itPlane++;
                        planes.vPlanes.erase(itPlane);

                        // Re-evaluate possible planes to merge
                        j--;
                        k = planes.vPlanes.size();
                    }
                }
#if _PRINT_PROFILING
    double time_end = pcl::getTime();
    cout << "Merge planes took " << double (time_end - time_start) << endl;
#endif

}

/*! Group the planes segmented from each single sensor into the common PbMap 'planes' */
void Spieye360::groupPlanes()
{
    //  cout << "groupPlanes...\n";
    //double time_start = pcl::getTime();

    float maxDistHull = 0.5;
    float maxDistParallelHull = 0.09;

    //    Eigen::Matrix4f Rt = calib->getRt_id(0);
    //    planes.MergeWith(local_planes_[0], Rt);
    planes = local_planes_[0];
    set<unsigned> prev_planes, first_planes;
    for(size_t i=0; i < planes.vPlanes.size(); i++)
        first_planes.insert(planes.vPlanes[i].id);
    prev_planes = first_planes;

    for(unsigned sensor_id=1; sensor_id < NUM_ASUS_SENSORS; ++sensor_id)
    {
        size_t j;
        set<unsigned> next_prev_planes;
        for(size_t k = 0; k < local_planes_[sensor_id].vPlanes.size(); k++)
        {
            bool bSamePlane = false;
            if(local_planes_[sensor_id].vPlanes[k].areaHull > 0.5 || local_planes_[sensor_id].vPlanes[k].curvature < max_curvature_plane)
                for(set<unsigned>::iterator it = prev_planes.begin(); it != prev_planes.end() && !bSamePlane; it++) // numPrevPlanes
                {
                    j = *it;

                    if(planes.vPlanes[j].areaHull < 0.5 || planes.vPlanes[j].curvature > max_curvature_plane)
                        continue;

                    Eigen::Vector3f close_points_diff;
                    //float prev_dist = 1;
                    if( fabs(planes.vPlanes[j].d - local_planes_[sensor_id].vPlanes[k].d) < 0.45 )
                        if( planes.vPlanes[j].v3normal.dot(local_planes_[sensor_id].vPlanes[k].v3normal) > 0.99 )
                        {
                            // Checking distances:
                            // a) Between an vertex and a vertex
                            // b) Between an edge and a vertex
                            // c) Between two edges (imagine two polygons on perpendicular planes)
                            //            if(!planes.vPlanes[j].isPlaneNearby(local_planes_[sensor_id].vPlanes[k],0.2);
                            //              continue;

                            for(unsigned i=1; i < planes.vPlanes[j].polygonContourPtr->size() && !bSamePlane; i++)
                                for(unsigned ii=1; ii < local_planes_[sensor_id].vPlanes[k].polygonContourPtr->size(); ii++)
                                {
                                    close_points_diff = mrpt::pbmap::diffPoints(planes.vPlanes[j].polygonContourPtr->points[i], local_planes_[sensor_id].vPlanes[k].polygonContourPtr->points[ii]);
                                    float dist = close_points_diff.norm();
                                    if( dist < maxDistHull && fabs(planes.vPlanes[j].v3normal.dot(close_points_diff)) < maxDistParallelHull)
                                    {
                                        bSamePlane = true;
                                        break;
                                    }
                                }
                            // a) & b)
                            if(!bSamePlane)
                                for(unsigned i=1; i < planes.vPlanes[j].polygonContourPtr->size() && !bSamePlane; i++)
                                    for(unsigned ii=1; ii < local_planes_[sensor_id].vPlanes[k].polygonContourPtr->size(); ii++)
                                    {
                                        float dist = sqrt(mrpt::pbmap::dist3D_Segment_to_Segment2(mrpt::pbmap::Segment(planes.vPlanes[j].polygonContourPtr->points[i],planes.vPlanes[j].polygonContourPtr->points[i-1]), mrpt::pbmap::Segment(local_planes_[sensor_id].vPlanes[k].polygonContourPtr->points[ii],local_planes_[sensor_id].vPlanes[k].polygonContourPtr->points[ii-1])));
                                        if( dist < maxDistHull)
                                        {
                                            close_points_diff = mrpt::pbmap::diffPoints(planes.vPlanes[j].polygonContourPtr->points[i], local_planes_[sensor_id].vPlanes[k].polygonContourPtr->points[ii]);
                                            if(fabs(planes.vPlanes[j].v3normal.dot(close_points_diff)) < maxDistParallelHull)
                                            {
                                                bSamePlane = true;
                                                break;
                                            }
                                        }
                                    }
                        }
                    if(bSamePlane)
                        break;
                }
            if( bSamePlane ) // The planes are merged if they are the same
            {
                next_prev_planes.insert(planes.vPlanes[j].id);
                planes.vPlanes[j].mergePlane2(local_planes_[sensor_id].vPlanes[k]);
            }
            else
            {
                next_prev_planes.insert(planes.vPlanes.size());
                local_planes_[sensor_id].vPlanes[k].id = planes.vPlanes.size();
                planes.vPlanes.push_back(local_planes_[sensor_id].vPlanes[k]);
            }
        }
        prev_planes = next_prev_planes;
        if(sensor_id == 6)
            prev_planes.insert(first_planes.begin(), first_planes.end());
    }
}

/*! This function segments planes from the point cloud corresponding to the sensor 'sensor_id',
      in its local frame of reference
  */
void Spieye360::segmentPlanesLocalCam(int sensor_id)
{
    // Segment planes
    //cout << sensor_id << " segmentPlanesLocalCam \n";
    //double extractPlanes_start = pcl::getTime();

    pcl::IntegralImageNormalEstimation<PointT, pcl::Normal> ne;
    ne.setNormalEstimationMethod (ne.COVARIANCE_MATRIX);
    ne.setMaxDepthChangeFactor (0.02); // For VGA: 0.02f, 10.0f
    ne.setNormalSmoothingSize (10.0f);
    ne.setDepthDependentSmoothing (true);

    pcl::OrganizedMultiPlaneSegmentation<PointT, pcl::Normal, pcl::Label> mps;
    mps.setMinInliers (100);
    mps.setAngularThreshold (0.039812); // (0.017453 * 2.0) // 3 degrees
    mps.setDistanceThreshold (0.02); //2cm

    pcl::PointCloud<pcl::Normal>::Ptr normal_cloud (new pcl::PointCloud<pcl::Normal>);
    ne.setInputCloud ( frameRGBD_[sensor_id].getPointCloud() );
    ne.compute (*normal_cloud);

    mps.setInputNormals (normal_cloud);
    mps.setInputCloud ( frameRGBD_[sensor_id].getPointCloud() );
    vector<pcl::PlanarRegion<PointT>, Eigen::aligned_allocator<pcl::PlanarRegion<PointT> > > regions;
    vector<pcl::ModelCoefficients> model_coefficients;
    vector<pcl::PointIndices> inlier_indices;
    pcl::PointCloud<pcl::Label>::Ptr labels (new pcl::PointCloud<pcl::Label>);
    vector<pcl::PointIndices> label_indices;
    vector<pcl::PointIndices> boundary_indices;
    mps.segmentAndRefine (regions, model_coefficients, inlier_indices, labels, label_indices, boundary_indices);
    //      mps.segment (model_coefficients, inlier_indices);
        cout << regions.size() << " planes detected\n";

    // Create a vector with the planes detected in this keyframe, and calculate their parameters (normal, center, pointclouds, etc.)
    for (size_t i = 0; i < regions.size (); i++)
    {
        //      cout << "curv " << regions[i].getCurvature() << endl;
        if(regions[i].getCurvature() > max_curvature_plane)
            continue;

        mrpt::pbmap::Plane plane;

        plane.v3center = regions[i].getCentroid ();
        plane.v3normal = Eigen::Vector3f(model_coefficients[i].values[0], model_coefficients[i].values[1], model_coefficients[i].values[2]);
        plane.d = model_coefficients[i].values[3];
        //        if( plane.v3normal.dot(plane.v3center) > 0)
        if( model_coefficients[i].values[3] < 0)
        {
            plane.v3normal = -plane.v3normal;
            plane.d = -plane.d;
        }
        plane.curvature = regions[i].getCurvature();
        //      cout << "normal " << plane.v3normal.transpose() << " center " << regions[i].getCentroid().transpose() << " " << plane.v3center.transpose() << endl;
        //    cout << "D " << -(plane.v3normal.dot(plane.v3center)) << " " << plane.d << endl;

        // Extract the planar inliers from the input cloud
        pcl::ExtractIndices<pcl::PointXYZRGBA> extract;
        extract.setInputCloud ( frameRGBD_[sensor_id].getPointCloud() );
        //        extract.setInputCloud ( cloud );
        extract.setIndices ( boost::make_shared<const pcl::PointIndices> (inlier_indices[i]) );
        extract.setNegative (false);
        extract.filter (*plane.planePointCloudPtr);    // Write the planar point cloud
        plane.inliers = inlier_indices[i].indices;
        //    cout << "Extract inliers\n";

        pcl::PointCloud<pcl::PointXYZRGBA>::Ptr contourPtr(new pcl::PointCloud<pcl::PointXYZRGBA>);
        contourPtr->points = regions[i].getContour();

        plane.computeInvariantParams( plane.planePointCloudPtr );
        plane.calcConvexHullandParams(contourPtr);

//        plane.calcConvexHull(contourPtr);
//        plane.computeMassCenterAndArea();
//        plane.calcElongationAndPpalDir();

        plane.calcPlaneHistH();

        // Check whether this region correspond to the same plane as a previous one (this situation may happen when there exists a small discontinuity in the observation)
        bool isSamePlane = false;
        for (size_t j = 0; j < local_planes_[sensor_id].vPlanes.size(); j++)
            if( local_planes_[sensor_id].vPlanes[j].isSamePlane(plane, 0.998, 0.1, 0.4) ) // The planes are merged if they are the same
            {
                //          cout << "Merge local region\n";
                isSamePlane = true;
                local_planes_[sensor_id].vPlanes[j].mergePlane(plane);

                break;
            }
        if(!isSamePlane)
        {
            //          plane.calcMainColor();
            plane.id = local_planes_[sensor_id].vPlanes.size();
            local_planes_[sensor_id].vPlanes.push_back(plane);
        }
    }
    //double extractPlanes_end = pcl::getTime();
    //cout << local_planes_[sensor_id].vPlanes.size() << " planes. Extraction in " << sensor_id << " took " << double (extractPlanes_end - extractPlanes_start) << endl;

    //      segmentation_im_[sensor_id] = true;
}


/*! This function segments planes from the point cloud corresponding to the sensor 'sensor_id',
      in the frame of reference of the omnidirectional camera
  */
void Spieye360::segmentPlanesSensor(int sensor_id)
{
    // Segment planes
    //    cout << "extractPlaneFeatures, size " << frameRGBD_[sensor_id].getPointCloud()->size() << "\n";

#if _PRINT_PROFILING
    double time_start = pcl::getTime();
#endif

    assert(frameRGBD_[sensor_id].getPointCloud()->height > 1 && frameRGBD_[sensor_id].getPointCloud()->width > 1);

    pcl::IntegralImageNormalEstimation<PointT, pcl::Normal> ne;
    //      ne.setNormalEstimationMethod (ne.SIMPLE_3D_GRADIENT);
    //      ne.setNormalEstimationMethod (ne.AVERAGE_DEPTH_CHANGE);
    ne.setNormalEstimationMethod (ne.AVERAGE_3D_GRADIENT);
    //      ne.setNormalEstimationMethod (ne.COVARIANCE_MATRIX);
    ne.setMaxDepthChangeFactor (0.02); // For VGA: 0.02f, 10.01
    ne.setNormalSmoothingSize (8.0f);
    ne.setDepthDependentSmoothing (true);

    pcl::OrganizedMultiPlaneSegmentation<PointT, pcl::Normal, pcl::Label> mps;
    //      mps.setMinInliers (max(uint32_t(40),frameRGBD_[sensor_id].getPointCloud()->height*2));
    mps.setMinInliers (80);
    mps.setAngularThreshold (0.039812); // (0.017453 * 2.0) // 3 degrees
    mps.setDistanceThreshold (0.02); //2cm
    //    cout << "PointCloud size " << frameRGBD_[sensor_id].getPointCloud()->size() << endl;

    pcl::PointCloud<pcl::Normal>::Ptr normal_cloud (new pcl::PointCloud<pcl::Normal>);
    ne.setInputCloud ( frameRGBD_[sensor_id].getPointCloud() );
    ne.compute (*normal_cloud);

    mps.setInputNormals (normal_cloud);
    mps.setInputCloud ( frameRGBD_[sensor_id].getPointCloud() );
    vector<pcl::PlanarRegion<PointT>, Eigen::aligned_allocator<pcl::PlanarRegion<PointT> > > regions;
    vector<pcl::ModelCoefficients> model_coefficients;
    vector<pcl::PointIndices> inlier_indices;
    pcl::PointCloud<pcl::Label>::Ptr labels (new pcl::PointCloud<pcl::Label>);
    vector<pcl::PointIndices> label_indices;
    vector<pcl::PointIndices> boundary_indices;
    mps.segmentAndRefine (regions, model_coefficients, inlier_indices, labels, label_indices, boundary_indices);

    // Create a vector with the planes detected in this keyframe, and calculate their parameters (normal, center, pointclouds, etc.)
    unsigned single_cloud_size = frameRGBD_[sensor_id].getPointCloud()->size();
    //Eigen::Matrix4f Rt = calib->getRt_id(sensor_id);//.cast<float>();
//    float angle_offset = 45;
//    Eigen::Matrix4f rot_offset = Eigen::Matrix4f::Identity(); rot_offset(1,1) = rot_offset(2,2) = cos(angle_offset*PI/180); rot_offset(1,2) = -sin(angle_offset*PI/180); rot_offset(2,1) = -rot_offset(1,2);
//    Eigen::Matrix4f Rt = rot_offset * calib->getRt_id(sensor_id);
    Eigen::Matrix4f Rt = calib->getRt_id(sensor_id);
    for (size_t i = 0; i < regions.size (); i++)
    {
        mrpt::pbmap::Plane plane;

        plane.v3center = regions[i].getCentroid ();
        plane.v3normal = Eigen::Vector3f(model_coefficients[i].values[0], model_coefficients[i].values[1], model_coefficients[i].values[2]);
        if( plane.v3normal.dot(plane.v3center) > 0)
        {
            plane.v3normal = -plane.v3normal;
            //          plane.d = -plane.d;
        }
        plane.curvature = regions[i].getCurvature ();
        //    cout << i << " getCurvature\n";

        //        if(plane.curvature > max_curvature_plane)
        //          continue;

        // Extract the planar inliers from the input cloud
        pcl::ExtractIndices<pcl::PointXYZRGBA> extract;
        extract.setInputCloud ( frameRGBD_[sensor_id].getPointCloud() );
        extract.setIndices ( boost::make_shared<const pcl::PointIndices> (inlier_indices[i]) );
        extract.setNegative (false);
        extract.filter (*plane.planePointCloudPtr);    // Write the planar point cloud
        plane.inliers.resize(inlier_indices[i].indices.size());
        for(size_t j=0; j<inlier_indices[i].indices.size(); j++)
            plane.inliers[j] = inlier_indices[i].indices[j] + sensor_id*single_cloud_size;

        pcl::PointCloud<pcl::PointXYZRGBA>::Ptr contourPtr(new pcl::PointCloud<pcl::PointXYZRGBA>);
        contourPtr->points = regions[i].getContour();

        plane.computeInvariantParams(plane.planePointCloudPtr);

        //    cout << "Extract contour\n";
        if(contourPtr->size() != 0)
        {
            //plane.calcConvexHull(contourPtr);
            plane.calcConvexHullandParams(contourPtr);
        }
        else
        {
            //        assert(false);
            cout << "HULL 000\n" << plane.planePointCloudPtr->size() << endl;
            static pcl::VoxelGrid<pcl::PointXYZRGBA> plane_grid;
            plane_grid.setLeafSize(0.05,0.05,0.05);
            plane_grid.setInputCloud (plane.planePointCloudPtr);
            plane_grid.filter (*contourPtr);
            //plane.calcConvexHull(contourPtr);
            plane.calcConvexHullandParams(contourPtr);
        }

////        //        assert(contourPtr->size() > 0);
//        plane.computeMassCenterAndArea();
////        //    cout << "Extract convexHull\n";
///
//        // Discard small planes
        if(plane.areaHull < min_area_plane)
            continue;

//        plane.d = -plane.v3normal .dot( plane.v3center );

//        plane.calcElongationAndPpalDir();
//        cout << "elongation " << plane.elongation << endl;
//        cout << "v3PpalDir " << plane.v3PpalDir.transpose() << endl;
//        plane.calcConvexHullandParams(contourPtr);
//        cout << "calcConvexHullandParams elongation " << plane.elongation << endl;
//        cout << "v3PpalDir " << plane.v3PpalDir.transpose() << endl;
//        mrpt::system::pause();

        // Discard narrow planes
        if(plane.elongation > max_elongation_plane)
            continue;

        //      double color_start = pcl::getTime();
        plane.calcPlaneHistH();
        plane.calcMainColor2();
        //      double color_end = pcl::getTime();
        //    cout << "color in " << (color_end - color_start)*1000 << " ms\n";

        //      color_start = pcl::getTime();
        plane.transform(Rt);
        //      color_end = pcl::getTime();
        //    cout << "transform in " << (color_end - color_start)*1000 << " ms\n";

        bool isSamePlane = false;
        if(plane.curvature < max_curvature_plane)
            for (size_t j = 0; j < local_planes_[sensor_id].vPlanes.size(); j++)
                if( local_planes_[sensor_id].vPlanes[j].curvature < max_curvature_plane && local_planes_[sensor_id].vPlanes[j].isSamePlane(plane, 0.99, 0.05, 0.2) ) // The planes are merged if they are the same
                {
                    //          cout << "Merge local region\n";
                    isSamePlane = true;
                    //            double time_start = pcl::getTime();
                    local_planes_[sensor_id].vPlanes[j].mergePlane2(plane);
                    //            double time_end = pcl::getTime();
                    //          cout << " mergePlane2 took " << double (time_start - time_end) << endl;

                    break;
                }
        if(!isSamePlane)
        {
            //          plane.calcMainColor();
            plane.id = local_planes_[sensor_id].vPlanes.size();
            local_planes_[sensor_id].vPlanes.push_back(plane);
        }
    }
#if _PRINT_PROFILING
    double extractPlanes_end = pcl::getTime();
    cout << "segmentPlanesInFrame in " << (extractPlanes_end - time_start)*1000 << " ms\n";
#endif

}

/*! Undistort the depth image corresponding to the sensor 'sensor_id' */
void Spieye360::undistortDepthSensor(int sensor_id)
{
#if _PRINT_PROFILING
    double time_start = pcl::getTime();
#endif

    //    frameRGBD_[sensor_id].loadDepthEigen();
    calib->intrinsic_model_[sensor_id].undistort(&frameRGBD_[sensor_id].m_depthEigUndistort);

#if _PRINT_PROFILING
    double time_end = pcl::getTime();
    cout << " undistort " << sensor_id << " took " << double (time_end - time_start)*10e3 << endl;
#endif

}

/*! Stitch both the RGB and the depth images corresponding to the sensor 'sensor_id' */
void Spieye360::stitchImage(int sensor_id)
{
    //The sensor 4 is looking forward in our robot platform, thus it is set in the center of the image
    Eigen::Vector3f virtualPoint, pointFromCamera;
    const int size_w = frameRGBD_[sensor_id].getRGBImage().cols;
    const int size_h = frameRGBD_[sensor_id].getRGBImage().rows;
    const float offsetPhi = sphereRGB.rows/2 - 0.5f;
    const float offsetTheta = -frameRGBD_[sensor_id].getRGBImage().rows*(NUM_ASUS_SENSORS-0.5f) + 0.5f;
    const float angle_pixel = 2*PI/sphereRGB.cols;
    const int marginStitching = 0;

    // Change reference system so that it coincides with the Spherical stereo system
    const int pixels_offset = (NUM_ASUS_SENSORS-3.5f) * frameRGBD_[sensor_id].getRGBImage().rows;

    cv::Vec3b *_rgb_dst = sphereRGB.ptr<cv::Vec3b>(0);
    unsigned short *_depth_dst = sphereDepth.ptr<unsigned short>(0);
    cv::Vec3b *_rgb_src = frameRGBD_[sensor_id].getRGBImage().ptr<cv::Vec3b>(0);
    unsigned short *_depth_src = frameRGBD_[sensor_id].getDepthImage().ptr<unsigned short>(0);

    for(int row_phi=0; row_phi < sphereRGB.rows; row_phi++)
    {
        int pixel_row = row_phi*sphereRGB.cols;
        float phi_i = (offsetPhi-row_phi) * angle_pixel;// + PI/2;
        virtualPoint(0) = sin(phi_i);
        float cos_phi = cos(phi_i);

        // Stitch each image from each sensor
        //        for(unsigned cam=0; cam < NUM_ASUS_SENSORS; cam++)
        //      #pragma omp parallel for
        int init_col_sphere = (NUM_ASUS_SENSORS-1-sensor_id)*size_h;
        int end_col_sphere = (NUM_ASUS_SENSORS-sensor_id)*size_h;
        if(sensor_id != 0)
            end_col_sphere += marginStitching;
        if(sensor_id != NUM_ASUS_SENSORS-1)
            init_col_sphere -= marginStitching;
        for(int col_theta=init_col_sphere; col_theta < end_col_sphere; col_theta++)
        {
            float theta_i = (col_theta+offsetTheta) * angle_pixel; // + PI;
            virtualPoint(1) = cos_phi*sin(theta_i);
            virtualPoint(2) = cos_phi*cos(theta_i);

            //          pointFromCamera = calib->Rt_10.block(0,0,3,3) * virtualPoint + calib->Rt_10.block(0,3,3,1);
            pointFromCamera = calib->Rt_inv[sensor_id].block(0,0,3,3) * virtualPoint + calib->Rt_inv[sensor_id].block(0,3,3,1);
            float u = calib->cameraMatrix(0,0) * pointFromCamera(0) / pointFromCamera(2) + calib->cameraMatrix(0,2);
            float v = calib->cameraMatrix(1,1) * pointFromCamera(1) / pointFromCamera(2) + calib->cameraMatrix(1,2);

            //          #pragma omp critical
            if(u >= 0 && u < size_w && v >= 0 && v < size_h)
            {
                int col_theta_ref4 = col_theta - pixels_offset;
                if(col_theta_ref4 < 0) col_theta_ref4 += sphereRGB.cols;
                //sphereRGB.at<cv::Vec3b>(row_phi,col_theta) = frameRGBD_[sensor_id].getRGBImage().at<cv::Vec3b>(v,u);
//                sphereRGB.at<cv::Vec3b>(row_phi,col_theta_ref4) = frameRGBD_[sensor_id].getRGBImage().at<cv::Vec3b>(v,u);
//                if( pcl_isfinite(frameRGBD_[sensor_id].getDepthImage().at<unsigned short>(v,u)) )
//                    sphereDepth.at<unsigned short>(row_phi,col_theta_ref4) = frameRGBD_[sensor_id].getDepthImage().at<unsigned short>(v,u) * sqrt(1 + pow((u-calib->cameraMatrix(0,2))/calib->cameraMatrix(0,0),2) + pow((v-calib->cameraMatrix(1,2))/calib->cameraMatrix(1,1),2));

                int pixel_dst = pixel_row + col_theta_ref4;
                int pixel_src = int(v)*frameRGBD_[sensor_id].getRGBImage().cols + int(u);
                _rgb_dst[pixel_dst] = _rgb_src[pixel_src];
                if( pcl_isfinite( _depth_src[pixel_src] ) )
                    _depth_dst[pixel_dst] = _depth_src[pixel_src] * sqrt(1 + pow((u-calib->cameraMatrix(0,2))/calib->cameraMatrix(0,0),2) + pow((v-calib->cameraMatrix(1,2))/calib->cameraMatrix(1,1),2));
            }
        }
    }
}

/*! Perform bilateral filtering on the point cloud
    */
void Spieye360::filterCloudBilateral_stereo()
{
#if _PRINT_PROFILING
    double time_start = pcl::getTime();
#endif

    // Filter pointClouds
    pcl::FastBilateralFilter<PointT> filter;
    //      filter.setSigmaS(10.0);
    //      filter.setSigmaR(0.05);
    filter.setSigmaS(20.0);
    filter.setSigmaR(0.2);
    filter.setInputCloud(sphereCloud);
    //      filter.filter(*filteredCloud);
    filter.filter(*sphereCloud);

#if _PRINT_PROFILING
    double time_end = pcl::getTime();
    cout << "filterCloudBilateral in " << (time_end - time_start)*1000 << " ms\n";
#endif
}


/*! This function segments planes from the point cloud
    */
void Spieye360::segmentPlanesStereo()
{
    // Segment planes
    //    cout << "extractPlaneFeatures, size " << sphereCloud->size() << "\n";

#if _PRINT_PROFILING
    double time_start = pcl::getTime();
#endif

    assert(sphereCloud->height > 1 && sphereCloud->width > 1);

    pcl::IntegralImageNormalEstimation<PointT, pcl::Normal> ne;
    //      ne.setNormalEstimationMethod (ne.SIMPLE_3D_GRADIENT);
    //      ne.setNormalEstimationMethod (ne.AVERAGE_DEPTH_CHANGE);
    ne.setNormalEstimationMethod (ne.AVERAGE_3D_GRADIENT);
    //      ne.setNormalEstimationMethod (ne.COVARIANCE_MATRIX);
    ne.setMaxDepthChangeFactor (0.1); // For VGA: 0.02f, 10.01
    ne.setNormalSmoothingSize (10.0f);
    ne.setDepthDependentSmoothing (true);

    pcl::OrganizedMultiPlaneSegmentation<PointT, pcl::Normal, pcl::Label> mps;
    //      mps.setMinInliers (max(uint32_t(40),sphereCloud->height*2));
    mps.setMinInliers (1000);
    mps.setAngularThreshold (0.07); // (0.017453 * 2.0 = 0.039812) // 3 degrees
    mps.setDistanceThreshold (0.1); //2cm
    //    cout << "PointCloud size " << sphereCloud->size() << endl;

    pcl::PointCloud<pcl::Normal>::Ptr normal_cloud (new pcl::PointCloud<pcl::Normal>);
    ne.setInputCloud ( sphereCloud );
    ne.compute (*normal_cloud);

    // Visualize normal map in RGB
//    cv::Mat normalMap = cv::Mat::zeros(sphereCloud->height, sphereCloud->width, CV_8UC3);;
//    computeNormalMap(normal_cloud, normalMap, true);

    mps.setInputNormals (normal_cloud);
    mps.setInputCloud ( sphereCloud );
    vector<pcl::PlanarRegion<PointT>, Eigen::aligned_allocator<pcl::PlanarRegion<PointT> > > regions;
    vector<pcl::ModelCoefficients> model_coefficients;
    vector<pcl::PointIndices> inlier_indices;
    pcl::PointCloud<pcl::Label>::Ptr labels (new pcl::PointCloud<pcl::Label>);
    vector<pcl::PointIndices> label_indices;
    vector<pcl::PointIndices> boundary_indices;
    mps.segmentAndRefine (regions, model_coefficients, inlier_indices, labels, label_indices, boundary_indices);

    // Create a vector with the planes detected in this keyframe, and calculate their parameters (normal, center, pointclouds, etc.)
    //unsigned single_cloud_size = sphereCloud->size();
    for (size_t i = 0; i < regions.size (); i++)
    {
        mrpt::pbmap::Plane plane;

        plane.v3center = regions[i].getCentroid ();
        plane.v3normal = Eigen::Vector3f(model_coefficients[i].values[0], model_coefficients[i].values[1], model_coefficients[i].values[2]);
        if( plane.v3normal.dot(plane.v3center) > 0)
        {
            plane.v3normal = -plane.v3normal;
            //          plane.d = -plane.d;
        }
        plane.curvature = regions[i].getCurvature ();
        //    cout << i << " getCurvature\n";

        //        if(plane.curvature > max_curvature_plane)
        //          continue;

        // Extract the planar inliers from the input cloud
        pcl::ExtractIndices<pcl::PointXYZRGBA> extract;
        extract.setInputCloud ( sphereCloud );
        extract.setIndices ( boost::make_shared<const pcl::PointIndices> (inlier_indices[i]) );
        extract.setNegative (false);
        extract.filter (*plane.planePointCloudPtr);    // Write the planar point cloud
        plane.inliers.resize(inlier_indices[i].indices.size());
        for(size_t j=0; j<inlier_indices[i].indices.size(); j++)
            plane.inliers[j] = inlier_indices[i].indices[j];

        pcl::PointCloud<pcl::PointXYZRGBA>::Ptr contourPtr(new pcl::PointCloud<pcl::PointXYZRGBA>);
        contourPtr->points = regions[i].getContour();

        //    cout << "Extract contour\n";
        if(contourPtr->size() != 0)
        {
            plane.calcConvexHull(contourPtr);
        }
        else
        {
            //        assert(false);
            cout << "HULL 000\n" << plane.planePointCloudPtr->size() << endl;
            static pcl::VoxelGrid<pcl::PointXYZRGBA> plane_grid;
            plane_grid.setLeafSize(0.05,0.05,0.05);
            plane_grid.setInputCloud (plane.planePointCloudPtr);
            plane_grid.filter (*contourPtr);
            plane.calcConvexHull(contourPtr);
        }

        //        assert(contourPtr->size() > 0);
        //        plane.calcConvexHull(contourPtr);
        //    cout << "calcConvexHull\n";
        plane.computeMassCenterAndArea();
        //    cout << "Extract convexHull\n";
        // Discard small planes
        if(plane.areaHull < min_area_plane)
            continue;

        plane.d = -plane.v3normal .dot( plane.v3center );

        plane.calcElongationAndPpalDir();
        // Discard narrow planes
        if(plane.elongation > max_elongation_plane)
            continue;

        //      double color_start = pcl::getTime();
        plane.calcPlaneHistH();
        plane.calcMainColor2();
        //      double color_end = pcl::getTime();
        //    cout << "color in " << (color_end - color_start)*1000 << " ms\n";

        //      color_start = pcl::getTime();
        //plane.transform(Rt);
        //      color_end = pcl::getTime();
        //    cout << "transform in " << (color_end - color_start)*1000 << " ms\n";

        bool isSamePlane = false;
        if(plane.curvature < max_curvature_plane)
            for (size_t j = 0; j < planes.vPlanes.size(); j++)
                if( planes.vPlanes[j].curvature < max_curvature_plane && planes.vPlanes[j].isSamePlane(plane, 0.99, 0.05, 0.2) ) // The planes are merged if they are the same
                {
                    //          cout << "Merge local region\n";
                    isSamePlane = true;
                    //            double time_start = pcl::getTime();
                    planes.vPlanes[j].mergePlane2(plane);
                    //            double time_end = pcl::getTime();
                    //          cout << " mergePlane2 took " << double (time_start - time_end) << endl;

                    break;
                }
        if(!isSamePlane)
        {
            //          plane.calcMainColor();
            plane.id = planes.vPlanes.size();
            planes.vPlanes.push_back(plane);
        }
    }
#if _PRINT_PROFILING
    double extractPlanes_end = pcl::getTime();
    cout << "segmentPlanesInFrame in " << (extractPlanes_end - time_start)*1000 << " ms\n";
#endif
    cout << "Planes " << planes.vPlanes.size() << " \n";

}

/*! This function segments planes from the point cloud corresponding to the sensor 'sensor_id',
      in the frame of reference of the omnidirectional camera
  */
void Spieye360::segmentPlanesStereoRANSAC()
{
    // Segment planes
    //    cout << "extractPlaneFeatures, size " << sphereCloud->size() << "\n";

#if _PRINT_PROFILING
    double time_start = pcl::getTime();
#endif

    pcl::PointCloud<PointT>::Ptr cloud_non_segmented (new pcl::PointCloud<PointT>);
    pcl::copyPointCloud (*sphereCloud, *cloud_non_segmented);

    // Create the segmentation object
    pcl::SACSegmentation<PointT> seg;
    seg.setOptimizeCoefficients (true); // Optional
    // Mandatory
    seg.setModelType (pcl::SACMODEL_PLANE);
    seg.setMethodType (pcl::SAC_RANSAC);
    seg.setDistanceThreshold (0.08);

    //        cout << "cloud_non_segmented " << cloud_non_segmented->size () << "\n";

    FilterPointCloud<PointT> filter(0.1);
    filter.filterVoxel(cloud_non_segmented);
    size_t min_cloud_segmentation = 0.2*cloud_non_segmented->size();

    // Create a vector with the planes detected in this keyframe, and calculate their parameters (normal, center, pointclouds, etc.)
    //unsigned single_cloud_size = sphereCloud->size();
    while (cloud_non_segmented->size() > min_cloud_segmentation )
    {
        cout << "cloud_non_segmented Pts " << cloud_non_segmented->size() << endl;

        pcl::ModelCoefficients::Ptr coefficients (new pcl::ModelCoefficients);
        pcl::PointIndices::Ptr inliers (new pcl::PointIndices);

        seg.setInputCloud (cloud_non_segmented);
        seg.segment (*inliers, *coefficients);

        cout << "Inliers " << inliers->indices.size () << "\n";

        if (inliers->indices.size () < 1000)
            break;

        mrpt::pbmap::Plane plane;

        // Extract the planar inliers from the input cloud
        pcl::ExtractIndices<PointT> extract;
        extract.setInputCloud ( cloud_non_segmented );
        extract.setIndices ( inliers );
        extract.setNegative (false);
        extract.filter (*plane.planePointCloudPtr);    // Write the planar point cloud
        extract.setNegative (true);
        extract.filter (*cloud_non_segmented);    // Write the planar point cloud
        plane.inliers = inliers->indices; // TODO: only the first pass of inliers is good, the next ones need to be re-arranged
        double center_x=0, center_y=0, center_z=0;
        for(size_t j=0; j<plane.inliers.size(); j++)
        {
            center_x += plane.planePointCloudPtr->points[plane.inliers[j] ].x;
            center_y += plane.planePointCloudPtr->points[plane.inliers[j] ].y;
            center_z += plane.planePointCloudPtr->points[plane.inliers[j] ].z;
        }

        plane.v3center = Eigen::Vector3f(center_x/plane.inliers.size(), center_y/plane.inliers.size(), center_z/plane.inliers.size());
        plane.v3normal = Eigen::Vector3f(coefficients->values[0], coefficients->values[1], coefficients->values[2]);
        if( plane.v3normal.dot(plane.v3center) > 0)
        {
            plane.v3normal = -plane.v3normal;
            //          plane.d = -plane.d;
        }
        plane.d = -plane.v3normal .dot( plane.v3center );

        //plane.curvature = regions[i].getCurvature ();
        //    cout << i << " getCurvature\n";


        pcl::PointCloud<pcl::PointXYZRGBA>::Ptr contourPtr(new pcl::PointCloud<pcl::PointXYZRGBA>);
        //            contourPtr->points = regions[i].getContour();

        //            //    cout << "Extract contour\n";
        static pcl::VoxelGrid<PointT> plane_grid;
        plane_grid.setLeafSize(0.05,0.05,0.05);
        plane_grid.setInputCloud (plane.planePointCloudPtr);
        plane_grid.filter (*contourPtr);
        plane.calcConvexHull(contourPtr);
        cout << "Inliers " << plane.planePointCloudPtr->size() << " hull " << plane.polygonContourPtr->size() << endl;

        //        assert(contourPtr->size() > 0);
        //        plane.calcConvexHull(contourPtr);
        //    cout << "calcConvexHull\n";
        plane.computeMassCenterAndArea();
        //    cout << "Extract convexHull\n";
        // Discard small planes
        if(plane.areaHull < min_area_plane)
            continue;

        plane.calcElongationAndPpalDir();
        // Discard narrow planes
        if(plane.elongation > max_elongation_plane)
            continue;

        //      double color_start = pcl::getTime();
        plane.calcPlaneHistH();
        plane.calcMainColor2();
        //          plane.calcMainColor();
        plane.id = planes.vPlanes.size();
        planes.vPlanes.push_back(plane);
    }
#if _PRINT_PROFILING
    double extractPlanes_end = pcl::getTime();
    cout << "segmentPlanesRANSAC took " << (extractPlanes_end - time_start)*1000 << " ms\n";
#endif
    cout << "Planes " << planes.vPlanes.size() << " \n";

}

/*! Compute the normalMap from an organized cloud of normal vectors. */
void Spieye360::computeNormalMap(const pcl::PointCloud<pcl::Normal>::Ptr & normal_cloud, cv::Mat & normalMap, const bool display)
{
    normalMap.create(sphereCloud->height, sphereCloud->width, CV_8UC3);
    for(size_t r=0; r < sphereCloud->height; ++r)
        for(size_t c=0; c < sphereCloud->width; ++c)
        {
            int i = r*sphereCloud->width + c;
    //            if( normal_cloud->points[i].normal_x == numeric_limits<float>::quiet_NaN () )
            if( normal_cloud->points[i].normal_x < 2.f )
            {
                normalMap.at<cv::Vec3b>(r,c)[2] = 255*(0.5*normal_cloud->points[i].normal_x+0.5);
                normalMap.at<cv::Vec3b>(r,c)[1] = 255*(0.5*normal_cloud->points[i].normal_y+0.5);
                normalMap.at<cv::Vec3b>(r,c)[0] = 255*(0.5*normal_cloud->points[i].normal_z+0.5);
    //            // Create a RGB image file with the values of the normal (multiply by 1.5 to increase the saturation to 0.5 = 1.5*0.33. Notice that normal vectors can be directly interpreted as normalized rgb, which has constant saturation of 0.33).
    //            normalMap.at<cv::Vec3b>(r,c)[2] = max(255., 255*(1.5f*fabs(normal_cloud->points[i].normal_x) ) );
    //            normalMap.at<cv::Vec3b>(r,c)[1] = max(255., 255*(1.5f*fabs(normal_cloud->points[i].normal_y) ) );
    //            normalMap.at<cv::Vec3b>(r,c)[0] = max(255., 255*(1.5f*fabs(normal_cloud->points[i].normal_z) ) );
            }
            //else
                //cout << "normal " << normal_cloud->points[i].normal_x << " " << normal_cloud->points[i].normal_y << " " << normal_cloud->points[i].normal_z << endl;
//                cv::Vec3b white(255,255,255);
//                for(i=0; i < 20; i++)
//                    normalMap.at<cv::Vec3b>(i) = white;
        }
    if(display)
    {
        cv::imshow("normalMap",normalMap);
        cv::imwrite("/Data/Results_IROS15/normalMap.png",normalMap);
        cv::waitKey();
        cv::destroyWindow("normalMap");
    }
}