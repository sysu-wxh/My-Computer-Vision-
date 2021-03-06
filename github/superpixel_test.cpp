// ****************************************************************************** 
// SEEDS Superpixels
// ******************************************************************************
// Author: Beat Kueng based on Michael Van den Bergh's code
// Contact: vamichae@vision.ee.ethz.ch
//
// This code implements the superpixel method described in:
// M. Van den Bergh, X. Boix, G. Roig, B. de Capitani and L. Van Gool, 
// "SEEDS: Superpixels Extracted via Energy-Driven Sampling",
// ECCV 2012
// 
// Copyright (c) 2012 Michael Van den Bergh (ETH Zurich). All rights reserved.
// ******************************************************************************

#include <vector>
#include <string>
#include "seeds2.h"
#include <cv.h>
#include <highgui.h>
#include <fstream>
#include <iostream>
#include <ctime>
#include "Matrix.h"
#include "Parameter.h"
#include "helper.h"
#include "EstimatePlane.h"
#include "Map.h"
using namespace std;

void makeSegmentBoundaryImage(const IplImage* inputImage,
	const IplImage* segmentImage,
	std::vector< std::vector<int> >& boundaryLabels,
	IplImage* segmentBoundaryImage);

string getNameFromPathWithoutExtension(string path) {
	string nameWith = path.substr(path.find_last_of("/\\") + 1);
	string nameWithout = nameWith.substr(0, nameWith.find_last_of("."));
	return nameWithout;
}

const double precision = 0.10;
vector<Matrix> pose;

int main(int argc, char* argv[]) {
	clock_t start, end;
	
	//************************
	//定义各类路径
	string root_str = "E://programs//dpdonwload//temp//temp//";
	string input_str = root_str + "image_2//";
	string pose_name = "E://programs//dpdonwload//temp//temp//poses//05.txt";  //pose文件
	string dispairty_str = root_str + "SPS//";
	string output_str1 = root_str + "SEEDResult//";    // 输出分割后图像
	string output_str2 = root_str + "SEEDBoundary//";   // 输出分割边界图
	string output_str3 = root_str + "SEEDLabel//";     //  输出分割seg文件
	string output_str4 = root_str + "planeReconstructionResult//"; // 输出最后的平面重建文件
	string output_str5 = root_str + "SEED//";    // 输出视差图优化图像


	//***********************
	//定义各种参数变量
	int NR_SUPERPIXELS = 266;   
	float disparity_factor = 0.0;

	int numlabels = 10;
	int width(0), height(0);
	//double cu = 607.1928, cv = 185.2157, f = 718.856, base = 0.54;
	double cu = 601.8873, cv = 183.1104, f = 707.091, base = 0.54;
	//double cu = 0.0, cv = 0.0, f = 1.0, base = 1.0;
	const int startNum = 0, imageNum = 20, imgStep = 1;
	char baseName[32];

	//****************
	//读取pose文件
	ifstream pose_file(pose_name.c_str(), ios::in);   // input pose file
	if (startNum == 0)    // 如果开始于第0帧
		pose.push_back(Matrix::eye(4));  //将一个四阶的单位矩阵压入pose向量中
	double temp00, temp01, temp02, temp03,   //定义了一个四阶的double变量
		   temp10, temp11, temp12, temp13,
	   	   temp20, temp21, temp22, temp23,
		   temp30 = 0.0, temp31 = 0.0, temp32 = 0.0, temp33 = 1.0;
	while (!pose_file.eof()) {  //旋转平移矩阵
		pose_file >> temp00 >> temp01 >> temp02 >> temp03
			>> temp10 >> temp11 >> temp12 >> temp13 
			>> temp20 >> temp21 >> temp22 >> temp23;
		Matrix tempM = Matrix::eye(4);
		tempM.val[0][0] = temp00, tempM.val[0][1] = temp01, tempM.val[0][2] = temp02, tempM.val[0][3] = temp03;
		tempM.val[1][0] = temp10, tempM.val[1][1] = temp11, tempM.val[1][2] = temp12, tempM.val[1][3] = temp13;
		tempM.val[2][0] = temp20, tempM.val[2][1] = temp21, tempM.val[2][2] = temp22, tempM.val[2][3] = temp23;
		tempM.val[3][0] = temp30, tempM.val[3][1] = temp31, tempM.val[3][2] = temp32, tempM.val[3][3] = temp33;
		pose.push_back(tempM);
	}
	cout << "Pose file read complete" << endl;

	//********************************************************************
	//以下开始求planecell
	Map map = Map();
	map.setPrecision(precision);    //设置map的精度
	string output_name4 = root_str + "output00_150.txt";  //设置最终输出的文件名
	for (int imageIndex = startNum; imageIndex < imageNum; imageIndex += imgStep) {  //从第startNum张开始，以imgStep的增长，截止到imageNum为止
		sprintf_s(baseName, "%06d.png", imageIndex);     //设置第几张图片，名称赋值给baseName
		cout << "processing number = " << baseName << endl;
		string input_file1 = input_str + baseName;    //设置要读取的image_2中的第imageIndex张图片
		IplImage* img = cvLoadImage(input_file1.c_str());  //加载图片给img
		string disparity_file = dispairty_str + baseName;   //设置要读取的视差图中对应的imageIndex张图片
		//cvNamedWindow("1");
		//cvShowImage("1", img);
		//cvWaitKey(0);
		IplImage* disparityImage = cvLoadImage(disparity_file.c_str(), -1);   //加载图片给disparityImage
		if ((!img) || (!disparityImage))   //如果有一个没读取到的话
		{
			printf("Error while opening %s\n", input_file1);
			return -1;
		}
		width = img->width;   //得到img的宽度
		height = img->height;  //得到img的高度
		std::cout << width << "+" << height << std::endl;
		int sz = height*width;   //得到总大小（也就是像素数）

		printf("Image loaded %d\n", img->nChannels);   //输出img图像的通道数

		UINT* ubuff = new UINT[sz];
		UINT* ubuff2 = new UINT[sz];
		UINT* dbuff = new UINT[sz];
		UINT pValue;   //rgb三颜色合为一个变量
		UINT pdValue;
		char c;
		UINT r, g, b, d = 0, dx, dy;
		int idx = 0;
		for (int j = 0; j < img->height; j++)
		{
			for (int i = 0; i < img->width; i++)
			{
				if (img->nChannels == 3)
				{
					// image is assumed to have data in BGR order
					b = ((uchar*)(img->imageData + img->widthStep*(j)))[(i)*img->nChannels];
					g = ((uchar*)(img->imageData + img->widthStep*(j)))[(i)*img->nChannels + 1];
					r = ((uchar*)(img->imageData + img->widthStep*(j)))[(i)*img->nChannels + 2];
					if (d < 128) d = 0;
					pValue = b | (g << 8) | (r << 16);
				}
				else if (img->nChannels == 1)
				{
					c = ((uchar*)(img->imageData + img->widthStep*(j)))[(i)*img->nChannels];
					pValue = c | (c << 8) | (c << 16);
				}
				else
				{
					printf("Unknown number of channels %d\n", img->nChannels);
					return -1;
				}
				ubuff[idx] = pValue;   //ubuff存了每个image_2图的RGB值（三合一）
				ubuff2[idx] = pValue;
				idx++;
			}
		}
			

		/*******************************************
		* SEEDS SUPERPIXELS                       *
		*******************************************/
		int NR_BINS = 5; // Number of bins in each histogram channel

		printf("Generating SEEDS with %d superpixels\n", NR_SUPERPIXELS);
		SEEDS seeds(width, height, 3, NR_BINS, disparity_factor);

		// SEEDS INITIALIZE
		int nr_superpixels = NR_SUPERPIXELS;

		// NOTE: the following values are defined for images from the BSD300 or BSD500 data set.
		// If the input image size differs from 480x320, the following values might no longer be 
		// accurate.
		// For more info on how to select the superpixel sizes, please refer to README.TXT.
		int seed_width = 3; int seed_height = 4; int nr_levels = 4;
		if (width >= height)
		{
			if (nr_superpixels == 600) { seed_width = 2; seed_height = 2; nr_levels = 4; }
			if (nr_superpixels == 400) { seed_width = 3; seed_height = 2; nr_levels = 4; }
			if (nr_superpixels == 266) { seed_width = 3; seed_height = 3; nr_levels = 4; }
			if (nr_superpixels == 200) { seed_width = 3; seed_height = 4; nr_levels = 4; }
			if (nr_superpixels == 150) { seed_width = 2; seed_height = 2; nr_levels = 5; }
			if (nr_superpixels == 100) { seed_width = 3; seed_height = 2; nr_levels = 5; }
			if (nr_superpixels == 50) { seed_width = 3; seed_height = 4; nr_levels = 5; }
			if (nr_superpixels == 25) { seed_width = 3; seed_height = 2; nr_levels = 6; }
			if (nr_superpixels == 17) { seed_width = 3; seed_height = 3; nr_levels = 6; }
			if (nr_superpixels == 12) { seed_width = 3; seed_height = 4; nr_levels = 6; }
			if (nr_superpixels == 9) { seed_width = 2; seed_height = 2; nr_levels = 7; }
			if (nr_superpixels == 6) { seed_width = 3; seed_height = 2; nr_levels = 7; }
		}
		else
		{
			if (nr_superpixels == 600) { seed_width = 2; seed_height = 2; nr_levels = 4; }
			if (nr_superpixels == 400) { seed_width = 2; seed_height = 3; nr_levels = 4; }
			if (nr_superpixels == 266) { seed_width = 3; seed_height = 3; nr_levels = 4; }
			if (nr_superpixels == 200) { seed_width = 4; seed_height = 3; nr_levels = 4; }
			if (nr_superpixels == 150) { seed_width = 2; seed_height = 2; nr_levels = 5; }
			if (nr_superpixels == 100) { seed_width = 2; seed_height = 3; nr_levels = 5; }
			if (nr_superpixels == 50) { seed_width = 4; seed_height = 3; nr_levels = 5; }
			if (nr_superpixels == 25) { seed_width = 2; seed_height = 3; nr_levels = 6; }
			if (nr_superpixels == 17) { seed_width = 3; seed_height = 3; nr_levels = 6; }
			if (nr_superpixels == 12) { seed_width = 4; seed_height = 3; nr_levels = 6; }
			if (nr_superpixels == 9) { seed_width = 2; seed_height = 2; nr_levels = 7; }
			if (nr_superpixels == 6) { seed_width = 2; seed_height = 3; nr_levels = 7; }
		}
		start = clock();
		seeds.initialize(seed_width, seed_height, nr_levels);//初始化lable  分割成想要的大小，并且统计了颜色直方图
		seeds.update_disparityImage(disparityImage);   //获得disparity_data数组，存储了视差图每个像素的灰度值

		//225
		seeds.update_image_ycbcr(ubuff);
		seeds.iterate();   //以上超像素结束，可以在这里改

		printf("SEEDS produced %d labels\n", seeds.count_superpixels());

		//创建一个EstimatePlane（直译为估计平面??）
		EstimatePlane estimatePlane;   //计算平面方程
		estimatePlane.setIterationTotal(outerIterationTotal, innerIterationTotal);
		estimatePlane.setWeightParameter(lambda_pos, lambda_depth, lambda_bou, lambda_smo);
		estimatePlane.setInlierThreshold(lambda_d);
		estimatePlane.setPenaltyParameter(lambda_hinge, lambda_occ, lambda_pen);


		//以下为得到planecell的过程
		IplImage* optimizedDisparityImage, *segmentImage;
		CvSize imgSize;   //读入图片的大小
		imgSize.width = width, imgSize.height = height;
		optimizedDisparityImage = cvCreateImage(imgSize, 8, 1);  //创建一个大小相同，深度为8，通道为1的图给optimizedDisparityImage
		segmentImage = cvCreateImage(imgSize, 16, 1);   //创建一个大小相同，深度为16，通道为1的图给segmentImage
		estimatePlane.planeCompute(disparityImage, seeds.count_superpixels(), seeds.labels[nr_levels - 1], segmentImage, optimizedDisparityImage);//核心

		double** planeFunction;   //give each pixel a plane function
		planeFunction = new double*[width*height];
		for (int i = 0; i < width*height; ++i)
			planeFunction[i] = new double[3];
		estimatePlane.getDisparityPlane(planeFunction);  
		end = clock();
		double elapsed_secs = double(end - start) / CLOCKS_PER_SEC;
		printf("time comsumption=%lf sec\n", elapsed_secs);

		//seeds.update_blocks_d(planeFunction);   // update blocks 应该在前面 这里更新了labels之后再进行平面分割
		//seeds.extractVertex(seeds.count_superpixels());
		seeds.findcontour();                                //here找superpixel的角点
		estimatePlane.performSmoothingSegmentation();  //
		estimatePlane.makeOutputImage(segmentImage, optimizedDisparityImage);
		//以上平面方程的计算


		//对planecell进行投影
		std::vector< std::vector<double> > disparityPlaneParameters;
		std::vector< std::vector<int> > boundaryLabels;
		estimatePlane.makeSegmentBoundaryData(disparityPlaneParameters, boundaryLabels);

		IplImage* segmentBoundaryImage;
		segmentBoundaryImage = cvCreateImage(imgSize, 8, 3);
		makeSegmentBoundaryImage(img, segmentImage, boundaryLabels, segmentBoundaryImage);
		//cvNamedWindow("Output");
		//cvNamedWindow("Disparity");
		//cvShowImage("Disparity", optimizedDisparityImage);
		//cvShowImage("Output", segmentBoundaryImage);
		//cvWaitKey(0);
		cvSaveImage((output_str5 + baseName).c_str(), optimizedDisparityImage);

		estimatePlane.cameraParameterInitialization(cu, cv, f, base);
		vector<VERTEX> vertexResult;
		vector<VERTEX3D> vertexProjectResult;
		//vertexProjectResult.resize(seeds.count_superpixels());
		seeds.planeCalculateVertex(vertexResult);
		estimatePlane.vertexProjectAndStore(vertexResult);    // 这里
		vertexResult.clear();
		estimatePlane.setInputColor(img);
		estimatePlane.projectPlane();
		vector<vector<double>> finalResult;
		vertexProjectResult = estimatePlane.getProjectResult(finalResult, pose[imageIndex]);     // 这里有问题
																								 // DRAW SEEDS OUTPUT
		sz = 3 * width*height;
		UINT* output_buff = new UINT[sz];
		for (int i = 0; i < sz; i++) output_buff[i] = 0;

		//printf("Draw Contours Around Segments\n");
		DrawContoursAroundSegments(ubuff, seeds.labels[nr_levels - 1], width, height, 0x0000ff, false);//0xff0000 draws red contours
		DrawContoursAroundSegments(output_buff, seeds.labels[nr_levels - 1], width, height, 0xffffff, true);//0xff0000 draws white contours
		string imageFileName = output_str1 + baseName;
		//printf("Saving image %s\n",imageFileName.c_str());
		SaveImage(ubuff, width, height,
			imageFileName.c_str());

		imageFileName = output_str2 + baseName;
		//printf("Saving image %s\n",imageFileName.c_str());
		SaveImage(output_buff, width, height,
			imageFileName.c_str());
		/*UINT* parentLabel;

		seeds.getParrentLabel(nr_levels-1, parentLabel);
		string parentLabelFileName = output_str3 + getNameFromPathWithoutExtension(string(input_file1)) + "_p.seg";
		ofstream output(parentLabelFileName.c_str(), ios::out);
		for (int i = 0; i < sizeof(parentLabel) / sizeof(parentLabel[0]); ++i) {
		output << parentLabel[i] << " ";
		}
		output << endl;
		output.close();*/
		string labelFileNameTxt = getNameFromPathWithoutExtension(string(input_file1));
		string parentLabelFileNameTxt = output_str3 + labelFileNameTxt + "_p.seg";

		for (int i = 0; i < finalResult.size(); ++i) {
			map.storeCluster(finalResult[i][0], finalResult[i][1], finalResult[i][2],
				finalResult[i][3], finalResult[i][4], finalResult[i][5],
				finalResult[i][6], finalResult[i][7], finalResult[i][8],
				vertexProjectResult[i]);
		}
		labelFileNameTxt = output_str3 + labelFileNameTxt + ".seg";
		seeds.SaveLabels_Text(labelFileNameTxt);

		//seeds.findcontour("sssss");
		//seeds.SaveParentLabels_Text(parentLabelFileNameTxt, nr_levels - 1);
		delete[] ubuff;
		delete[] output_buff;

		//estimatePlane.freeBuffer();
	}
	map.outputFile(output_name4);
	pose_file.close();
	printf("Done!\n");
	system("pause");
	return 0;
}


void makeSegmentBoundaryImage(const IplImage* inputImage,
	const IplImage* segmentImage,
	std::vector< std::vector<int> >& boundaryLabels,
	IplImage* segmentBoundaryImage)
{
	int width = static_cast<int>(inputImage->width);
	int height = static_cast<int>(inputImage->height);
	int boundaryTotal = static_cast<int>(boundaryLabels.size());

	//segmentBoundaryImage.resize(width, height);
	/*for (int y = 0; y < height; ++y) {
	for (int x = 0; x < width; ++x) {
	//segmentBoundaryImage.set_pixel(x, y, inputImage.get_pixel(x, y));
	cvSet2D(segmentBoundaryImage, y, x, cvGet2D(inputImage, y, x));
	}
	}*/
	cvCopy(inputImage, segmentBoundaryImage, NULL);

	/*cvNamedWindow("Original Segment Boundary Image");
	cvShowImage("Original Segment Boundary Image",segmentBoundaryImage);
	cvWaitKey(100);
	printf("Original Segment Boundary Image已显示\n");*/

	int boundaryWidth = 2;
	CvScalar temp_pixel;
	for (int y = 0; y < height - 1; ++y) {
		for (int x = 0; x < width - 1; ++x) {
			CvScalar pixelLabelIndex = cvGet2D(segmentImage, y, x);
			temp_pixel.val[0] = 128, temp_pixel.val[1] = 128, temp_pixel.val[2] = 128;

			if (cvGet2D(segmentImage, y, x + 1).val[0] != pixelLabelIndex.val[0]) {
				for (int w = 0; w < boundaryWidth - 1; ++w) {
					if (x - w >= 0) {
						cvSet2D(segmentBoundaryImage, y, x - w, temp_pixel);
					}
				}
				for (int w = 1; w < boundaryWidth; ++w) {
					if (x + w < width) {
						cvSet2D(segmentBoundaryImage, y, x + w, temp_pixel);
					}
				}
			}
			if (cvGet2D(segmentImage, y + 1, x).val[0] != pixelLabelIndex.val[0]) {
				for (int w = 0; w < boundaryWidth - 1; ++w) {
					if (y - w >= 0) {
						cvSet2D(segmentBoundaryImage, y - w, x, temp_pixel);
					}
				}
				for (int w = 1; w < boundaryWidth; ++w) {
					if (y + w < height) {
						cvSet2D(segmentBoundaryImage, y + w, x, temp_pixel);
					}
				}
			}
		}
	}

	boundaryWidth = 3;
	for (int y = 0; y < height - 1; ++y) {
		for (int x = 0; x < width - 1; ++x) {
			//int pixelLabelIndex = segmentImage.get_pixel(x, y);
			CvScalar pixelLabelIndex = cvGet2D(segmentImage, y, x);

			if (cvGet2D(segmentImage, y, x + 1).val[0] != pixelLabelIndex.val[0]) {
				//png::rgb_pixel negativeSideColor, positiveSideColor;
				CvScalar negativeSideColor, positiveSideColor;
				int pixelBoundaryIndex = -1;
				for (int boundaryIndex = 0; boundaryIndex < boundaryTotal; ++boundaryIndex) {  //为什么可以换一下位置
					if ((boundaryLabels[boundaryIndex][0] == pixelLabelIndex.val[0] && boundaryLabels[boundaryIndex][1] == cvGet2D(segmentImage, y, x + 1).val[0])
						|| (boundaryLabels[boundaryIndex][0] == cvGet2D(segmentImage, y, x + 1).val[0] && boundaryLabels[boundaryIndex][1] == pixelLabelIndex.val[0]))
					{
						pixelBoundaryIndex = boundaryIndex;
						break;
					}
				}
				if (boundaryLabels[pixelBoundaryIndex][2] == 3) continue;
				else if (boundaryLabels[pixelBoundaryIndex][2] == 2) {   //B G R
					negativeSideColor.val[0] = 0;  negativeSideColor.val[1] = 225;  negativeSideColor.val[2] = 0;
					positiveSideColor.val[0] = 0;  positiveSideColor.val[1] = 225;  positiveSideColor.val[2] = 0;
				}
				else if (pixelLabelIndex.val[0] == boundaryLabels[pixelBoundaryIndex][boundaryLabels[pixelBoundaryIndex][2]]) {
					//negativeSideColor.red = 225;  negativeSideColor.green = 0;  negativeSideColor.blue = 0;
					//positiveSideColor.red = 0;  positiveSideColor.green = 0;  positiveSideColor.blue = 225;
					negativeSideColor.val[0] = 0;  negativeSideColor.val[1] = 0;  negativeSideColor.val[2] = 225;
					positiveSideColor.val[0] = 225;  positiveSideColor.val[1] = 0;  positiveSideColor.val[2] = 0;
				}
				else {
					//negativeSideColor.red = 0;  negativeSideColor.green = 0;  negativeSideColor.blue = 225;
					//positiveSideColor.red = 225;  positiveSideColor.green = 0;  positiveSideColor.blue = 0;
					negativeSideColor.val[0] = 225;  negativeSideColor.val[1] = 0;  negativeSideColor.val[2] = 0;
					positiveSideColor.val[0] = 0;  positiveSideColor.val[1] = 0;  positiveSideColor.val[2] = 225;
				}

				for (int w = 0; w < boundaryWidth - 1; ++w) {
					if (x - w >= 0) cvSet2D(segmentBoundaryImage, y, x - w, negativeSideColor);
				}
				for (int w = 1; w < boundaryWidth; ++w) {
					if (x + w < width) cvSet2D(segmentBoundaryImage, y, x + w, positiveSideColor);
				}
			}
			if (cvGet2D(segmentImage, y + 1, x).val[0] != pixelLabelIndex.val[0]) {
				CvScalar negativeSideColor, positiveSideColor;
				int pixelBoundaryIndex = -1;
				for (int boundaryIndex = 0; boundaryIndex < boundaryTotal; ++boundaryIndex) {
					if ((boundaryLabels[boundaryIndex][0] == pixelLabelIndex.val[0] && boundaryLabels[boundaryIndex][1] == cvGet2D(segmentImage, y + 1, x).val[0])
						|| (boundaryLabels[boundaryIndex][0] == cvGet2D(segmentImage, y + 1, x).val[0] && boundaryLabels[boundaryIndex][1] == pixelLabelIndex.val[0]))
					{
						pixelBoundaryIndex = boundaryIndex;
						break;
					}
				}
				if (boundaryLabels[pixelBoundaryIndex][2] == 3) continue;
				else if (boundaryLabels[pixelBoundaryIndex][2] == 2) {
					negativeSideColor.val[0] = 0;  negativeSideColor.val[1] = 225;  negativeSideColor.val[2] = 0;
					positiveSideColor.val[0] = 0;  positiveSideColor.val[1] = 225;  positiveSideColor.val[2] = 0;
				}
				else if (pixelLabelIndex.val[0] == boundaryLabels[pixelBoundaryIndex][boundaryLabels[pixelBoundaryIndex][2]]) {
					negativeSideColor.val[0] = 0;  negativeSideColor.val[1] = 0;  negativeSideColor.val[2] = 225;
					positiveSideColor.val[0] = 225;  positiveSideColor.val[1] = 0;  positiveSideColor.val[2] = 0;
				}
				else {
					negativeSideColor.val[0] = 225;  negativeSideColor.val[1] = 0;  negativeSideColor.val[2] = 0;
					positiveSideColor.val[0] = 0;  positiveSideColor.val[1] = 0;  positiveSideColor.val[2] = 225;
				}

				for (int w = 0; w < boundaryWidth - 1; ++w) {
					if (y - w >= 0) cvSet2D(segmentBoundaryImage, y - w, x, negativeSideColor);
				}
				for (int w = 1; w < boundaryWidth; ++w) {
					if (y + w < height) cvSet2D(segmentBoundaryImage, y + w, x, positiveSideColor);
				}
			}
		}
	}
}