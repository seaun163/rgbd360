IF(BUILD_REGISTRATION)

	INCLUDE_DIRECTORIES( ${OpenCV_INCLUDE_DIRS} )

	# Declare the target (an executable)
	ADD_EXECUTABLE(RegisterPairRGBD360 RegisterPairRGBD360.cpp
						../OpenNI2_Grabber/third_party/CLAMS/discrete_depth_distortion_model.cpp
						../OpenNI2_Grabber/third_party/CLAMS/include/clams/discrete_depth_distortion_model.h
						../include/Miscellaneous.h
						../include/Calib360.h
						../include/Frame360.h
						../include/Map360_Visualizer.h
						../include/RegisterRGBD360.h
	)

	TARGET_LINK_LIBRARIES(RegisterPairRGBD360
					${MRPT_LIBS}
					${OpenCV_LIBS}
					${PCL_LIBRARIES}
					${Boost_SERIALIZATION_LIBRARY}
					   )

#	# Declare the target (an executable)
#	ADD_EXECUTABLE(RegisterRGBD_dataset RegisterRGBD_dataset.cpp
#					../include/Miscellaneous.h
#					../include/RegisterPhotoICP.h
#					../third_party/cvShowManyImages.h
#	)
#
#	TARGET_LINK_LIBRARIES(RegisterRGBD_dataset
#					${MRPT_LIBS}
#					${OpenCV_LIBS}
#					${PCL_LIBRARIES}
#					${Boost_SERIALIZATION_LIBRARY}
#					   )

	# Declare the target (an executable)
	ADD_EXECUTABLE(MethodsRegisterRGBD360 MethodsRegisterRGBD360.cpp
						../OpenNI2_Grabber/third_party/CLAMS/discrete_depth_distortion_model.cpp
						../OpenNI2_Grabber/third_party/CLAMS/include/clams/discrete_depth_distortion_model.h
						../include/Miscellaneous.h
						../include/Calib360.h
						../include/Frame360.h
						../include/Map360_Visualizer.h
						../include/RegisterRGBD360.h
						../include/RegisterPhotoICP.h
	)

	TARGET_LINK_LIBRARIES(MethodsRegisterRGBD360
					${MRPT_LIBS}
					${OpenCV_LIBS}
					${PCL_LIBRARIES}
					${Boost_SERIALIZATION_LIBRARY}
					   )

	# Declare the target (an executable)
	ADD_EXECUTABLE(OdometryRGBD360 OdometryRGBD360.cpp
						../OpenNI2_Grabber/third_party/CLAMS/discrete_depth_distortion_model.cpp
						../OpenNI2_Grabber/third_party/CLAMS/include/clams/discrete_depth_distortion_model.h
						../include/Miscellaneous.h
						../include/Calib360.h
						../include/Frame360.h
						../include/Map360.h
						../include/RegisterRGBD360.h
	)

	TARGET_LINK_LIBRARIES(OdometryRGBD360
					${MRPT_LIBS}
					${OpenCV_LIBS}
					${PCL_LIBRARIES}
					${Boost_SERIALIZATION_LIBRARY}
					   )

	# Declare the target (an executable)
	ADD_EXECUTABLE(OdometryKeyFrame360 OdometryKeyFrame360.cpp
						../OpenNI2_Grabber/third_party/CLAMS/discrete_depth_distortion_model.cpp
						../OpenNI2_Grabber/third_party/CLAMS/include/clams/discrete_depth_distortion_model.h
						../include/Miscellaneous.h
						../include/Calib360.h
						../include/Frame360.h
						../include/Map360.h
						../include/RegisterRGBD360.h
	)

	TARGET_LINK_LIBRARIES(OdometryKeyFrame360
					${MRPT_LIBS}
					${OpenCV_LIBS}
					${PCL_LIBRARIES}
					${Boost_SERIALIZATION_LIBRARY}
					   )

#	# Declare the target (an executable)
#	ADD_EXECUTABLE(RegisterSequenceLabel RegisterSequenceSphere_labelFast.cpp
#						../OpenNI2_Grabber/third_party/CLAMS/discrete_depth_distortion_model.cpp
#						../OpenNI2_Grabber/third_party/CLAMS/include/clams/discrete_depth_distortion_model.h
#						../include/Miscellaneous.h
#						../include/Calib360.h
#						../include/Frame360.h
#	)
#
#	TARGET_LINK_LIBRARIES(RegisterSequenceLabel
#					${MRPT_LIBS}
#					${OpenCV_LIBS}
#					${PCL_LIBRARIES}
#					${Boost_SERIALIZATION_LIBRARY}
#					   )
#
#	# Declare the target (an executable)
#	ADD_EXECUTABLE(OnlineOdometryRGBD360 OnlineOdometryRGBD360.cpp
#						../include/CloudGrabber.h
#						../include/Miscellaneous.h
#						../include/Calib360.h
#						../include/Frame360.h
#						../OpenNI2_Grabber/third_party/CLAMS/include/clams/discrete_depth_distortion_model.h
#						../OpenNI2_Grabber/third_party/CLAMS/discrete_depth_distortion_model.cpp
#	)
#
#	TARGET_LINK_LIBRARIES(OnlineOdometryRGBD360
#						${MRPT_LIBS}
#						${OpenCV_LIBS}
#						${PCL_LIBRARIES}
#						${Boost_SERIALIZATION_LIBRARY}
#					   )

ENDIF(BUILD_REGISTRATION)
