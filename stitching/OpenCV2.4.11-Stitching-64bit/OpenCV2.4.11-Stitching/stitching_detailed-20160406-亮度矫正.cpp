﻿#include <iostream>
#include <fstream>
#include <string>
#include "opencv2/opencv_modules.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/stitching/detail/autocalib.hpp"
#include "opencv2/stitching/detail/blenders.hpp"
#include "opencv2/stitching/detail/camera.hpp"
#include "opencv2/stitching/detail/exposure_compensate.hpp"
#include "opencv2/stitching/detail/matchers.hpp"
#include "opencv2/stitching/detail/motion_estimators.hpp"
#include "opencv2/stitching/detail/seam_finders.hpp"
#include "opencv2/stitching/detail/util.hpp"
#include "opencv2/stitching/detail/warpers.hpp"
#include "opencv2/stitching/warpers.hpp"
#include  <windows.h> 
#include "stitching_sharemem.h"

using namespace std;
using namespace cv;
using namespace cv::detail;

HANDLE hMutex;
HANDLE hEventPreStitch;
HANDLE hMutexMessg;
//排序图片 从1到n 必须从做到有 1 到 N 
char outbuf[50];
char testfile[50];
int num_images = 3 ;// camera numbers 
bool Upflag = false;
bool imgMatready=false;
vector<Mat> sharedmat(num_images);
/////////////////////////////////////////////////////////////////////////////公用变量
//第一图像宽高和img mask
vector<VideoCapture> capture(num_images);
vector<Mat>	xmapstitch(num_images), ymapstitch(num_images);
vector<Mat>	xmap1(num_images), ymap1(num_images); 
vector<Mat> mapEye1(num_images), mapEye2(num_images);
int loopNum = 0;
////////////////////////////////////////////////////////////////////////////预处理变量
Mat result1, result_mask1; 
Mat  xmapdet, ymapdet,indxmap , indxmap2;
vector<Rect> dst_roi(num_images);
vector<Point> corners(num_images);
Rect dst_roi_1 ;
int srctype = 0;
////////////////////////////////////////////////////////////////////////////拼接变量
Mat ResultStitch;
Mat Result_mask_stitch;
Mat  xmapdetstitch, ymapdetstitch,indxmapstitch;
vector<Mat> img4(num_images);

wchar_t		OpMutex0[]	 = L"SM_MutexRTSPData0";
wchar_t		OpShareMem0[] = L"ShareMemoryRTSPData0";
wchar_t		OpServerOver0[]	 = L"ServerWriteOver0";
wchar_t		OpClientOver0[]	 = L"ClientReadOver0";

wchar_t		OpMutex[]	 = L"SM_MutexRTSPData1";
wchar_t		OpShareMem[] = L"ShareMemoryRTSPData1";
wchar_t		OpServerOver[]	 = L"ServerWriteOver1";
wchar_t		OpClientOver[]	 = L"ClientReadOver1";

wchar_t		OpMutex2[]	 = L"SM_MutexRTSPData2";
wchar_t		OpShareMem2[] = L"ShareMemoryRTSPData2";
wchar_t		OpServerOver2[]	 = L"ServerWriteOver2";
wchar_t		OpClientOver2[]	 = L"ClientReadOver2";

Stitching_sharemem shareMem[3] ={Stitching_sharemem(OpMutex0,OpShareMem0,OpServerOver0,OpClientOver0 ) ,
								 Stitching_sharemem(OpMutex ,OpShareMem ,OpServerOver ,OpClientOver  ) , 
								 Stitching_sharemem(OpMutex2,OpShareMem2,OpServerOver2,OpClientOver2 ) };


Rect resultRoiSize(const vector<Point> &corners, const vector<Size> &sizes)
{
    CV_Assert(sizes.size() == corners.size());
    Point tl(numeric_limits<int>::max(), numeric_limits<int>::max());
    Point br(numeric_limits<int>::min(), numeric_limits<int>::min());
    for (size_t i = 0; i < corners.size(); ++i)
    {
        tl.x = min(tl.x, corners[i].x);
        tl.y = min(tl.y, corners[i].y);
        br.x = max(br.x, corners[i].x + sizes[i].width);
        br.y = max(br.y, corners[i].y + sizes[i].height);
    }
    return Rect(tl, br);
}

void prepareSize(Rect dst_roi , Mat &dst_ , Mat &dst_mask_ , Rect &dst_roi_)
{
    dst_.create(dst_roi.size(), CV_16SC3);
    dst_.setTo(Scalar::all(0));
    dst_mask_.create(dst_roi.size(), CV_8U);
    dst_mask_.setTo(Scalar::all(0));
    dst_roi_ = dst_roi;

	xmapdet.create(dst_roi.size(), CV_32F);
	xmapdet.setTo(Scalar::all(0));
	ymapdet.create(dst_roi.size(), CV_32F);
	ymapdet.setTo(Scalar::all(0));
	indxmap.create(dst_roi.size(), CV_32F);
	indxmap.setTo(Scalar::all(0));

	//indxmap2.create(dst_roi.size(), CV_32F);
	//indxmap2.setTo(Scalar::all(-1));
}

//vector<Mat> imgmask(num_images);
//int flagmask = 0;
void feedSize(const Mat &img, const Mat &mask, Point tl , Rect &dst_roi_ ,Mat &dst_ , Mat &dst_mask_ ,int imgidx )
{
    CV_Assert(img.type() == CV_16SC3);
    CV_Assert(mask.type() == CV_8U);
    int dx = tl.x - dst_roi_.x;
    int dy = tl.y - dst_roi_.y;
///////////////////////////////////////////////////////////////////////
//	if(imgmask[imgidx].data == NULL )
//	{
//		imgmask[imgidx].create(mask.size(), CV_32FC3);
//	}
//	flagmask = 0;
/////////////////////////////////////////////////////////////////////
    for (int y = 0; y < img.rows ; ++y) //img.rows
    {
        const Point3_<short> *src_row = img.ptr<Point3_<short> >(y);
        Point3_<short> *dst_row = dst_.ptr<Point3_<short> >(dy + y);
        const uchar *mask_row = mask.ptr<uchar>(y);
        uchar *dst_mask_row = dst_mask_.ptr<uchar>(dy + y);

		//uchar *maks_sti_row = imgmask[imgidx].ptr<uchar>(y);

		float *dst_x = xmapdet.ptr<float>(dy + y);
		float *dst_y = ymapdet.ptr<float>(dy + y);
		float *dst_ix = indxmap.ptr<float>(dy + y);
		//float *dst_ix2 = indxmap2.ptr<float>(dy + y);

        for (int x = 0; x < img.cols; ++x)  //img.cols
        {
            if (mask_row[x])
			{
                dst_row[dx + x] = src_row[x];            

				dst_x[dx + x] = (float)x;
				dst_y[dx + x] = (float)y;
				dst_ix[dx + x] = (float)imgidx;	
			}
			//else
			//{
				//if (dst_ix[dx + x] == -1)
				//	dst_ix[dx + x] = (float)imgidx;		
				//else
				//	dst_ix2[dx + x] = (float)imgidx;
			//}
			dst_mask_row[dx + x] |= mask_row[x];

/////////////////////////////////////////////////////////////////////////////////
#if 0

            if (( mask_row[x] == 1)&&(flagmask == 0 ))  // 0 - > 1
			{
				if( imgidx != 0 )  //去掉第一幅左边
				{
					for( int i = 0  ;  i < x  ; i++ )
					{

						maks_sti_row[i] = i/x*0.5;		// 扩大100倍			
					}
				}
				//maks_sti_row[x] = 1;
			}
			else if (( mask_row[x] == 0)&&(flagmask == 1)) // 1 -> 0
			{
				if( imgidx != ( num_images - 1 ) ) //去掉最后幅右边
				{
					for( int i = x ;  i < img.cols  ; i++ )
					{
						int w = img.cols;
						maks_sti_row[i] = (w - i)/(w - x)*0.5;		////扩大100倍，需要最后除以100			
					
					}	
				}
			}
			else if (( mask_row[x] == 1)&&(flagmask == 1))
				maks_sti_row[x] = 1;
			
			flagmask = mask_row[x];		
#endif 
//////////////////////////////////////////////////////////////////////////////////
        }
    }
		//namedWindow("MyTest",0);
		//imshow( "MyTest", imgmask[imgidx] );
		//waitKey(30);
}

void feedSizeRemap(const vector<Mat> &img, Mat &dst_ , Mat &dst_mask_  )
{
#if 0
    for (int y = 0; y < dst_.rows ; ++y)  
    {
        Point3_<short> *dst_row = dst_.ptr<Point3_<short> >( y );
        uchar *dst_mask_row = dst_mask_.ptr<uchar>( y );

		float *dst_x = xmapdetstitch.ptr<float>( y );
		float *dst_y = ymapdetstitch.ptr<float>( y );
		float *dst_z = indxmapstitch.ptr<float>( y );

        for (int x = 0; x < dst_.cols; ++x)   
        {
			if (dst_mask_row[x])
			{	
				dst_row[x] = img[dst_z[x]].at< Point3_<short> >(  (int)dst_y[x] , (int)dst_x[x]  ) ;			
			}
        }
    }
#else   //裁剪上下边框的算法
    for (int y = (dst_.rows*0.1) ; y < (dst_.rows*0.9) ; ++y)  
    {
        Point3_<short> *dst_row = dst_.ptr<Point3_<short> >( y );
        uchar *dst_mask_row = dst_mask_.ptr<uchar>( y );

		float *dst_x = xmapdetstitch.ptr<float>( y );
		float *dst_y = ymapdetstitch.ptr<float>( y );
		float *dst_z = indxmapstitch.ptr<float>( y );

        for (int x = 10; x < (dst_.cols - 10); ++x)   
        {
			if (dst_mask_row[x])
			{	
				dst_row[x] = img[dst_z[x]].at< Point3_<short> >( (int)dst_y[x] , (int)dst_x[x]  ) ;			
			}
        }
    }
#endif 
}

void blendSize(Mat &dst, Mat &dst_mask , Mat &dst_ , Mat &dst_mask_ )
{
    dst_.setTo(Scalar::all(0), dst_mask_ == 0);
    dst = dst_;
    dst_mask = dst_mask_;
//    dst_.release();
//    dst_mask_.release();
}

Rect warpsize( RotationWarper * warper  , Mat &src,  Mat &K, const Mat &R, int interp_mode, int border_mode,
                                  Mat &dst , Mat &xmap, Mat &ymap )
{

    Rect dst_roi = warper->buildMaps(src.size(), K, R, xmap, ymap);

    dst.create(dst_roi.height + 1, dst_roi.width + 1, src.type());
    remap(src, dst, xmap, ymap, interp_mode, border_mode);

    return dst_roi;
}


int RunTestEyeFishMap( Size sizedata , int img_index)
{
	Mat im1, D1;
	im1.create(3,3,CV_64FC1); 
	D1.create(1,4,CV_64FC1);

    double fc1,fc2,cc1,cc2,kc1,kc2,kc3,kc4;

	fc1 = 655.6;
    fc2 = 656.4;
    cc1 = 625.06;
    cc2 = 361.96;
    kc1 = -0.382;
    kc2 = 0.195;
    kc3 = -0.00363;
    kc4 = 0.00237;

	im1.at<double>(0, 0) = fc1;
	im1.at<double>(0, 1) = 0;
	im1.at<double>(0, 2) = cc1;
	im1.at<double>(1, 0) = 0;
	im1.at<double>(1, 1) = fc2;
	im1.at<double>(1, 2) = cc2;
	im1.at<double>(2, 0) = 0;
	im1.at<double>(2, 1) = 0;
	im1.at<double>(2, 2) = 1;
	D1.at<double>(0, 0) = kc1;
	D1.at<double>(0, 1) = kc2;
	D1.at<double>(0, 2) = kc3;
	D1.at<double>(0, 3) = kc4;

    initUndistortRectifyMap(im1, D1, Mat(),im1 , sizedata , CV_16SC2, mapEye1[img_index], mapEye2[img_index]);
    //remap( Img, ImgUndistort, mapEye1[img_index], mapEye2[img_index] , INTER_LINEAR );

	im1.release();
	D1.release();
	return 0;
}

vector<Mat> KcameraB(num_images);
vector<CameraParams> camerasB;
vector<CameraParams> cameras(num_images);

Ptr<ExposureCompensator> compensator;

DWORD WINAPI GetPreStruct( LPVOID lpParameter )
{
    int64 app_start_time = getTickCount();
	int frmnum = 1;
    cv::setBreakOnError(true);

	// Default command line args
	vector<string> img_names;
	double work_megapix = 0.6; //如果是1会造成矫正时间过长，速度太大 	(*adjuster)(features, pairwise_matches, cameras);
	double seam_megapix = 0.1;
	double compose_megapix = -1;
	float conf_thresh = 0.6f;

	string ba_refine_mask = "xxxxx";
	bool do_wave_correct = true;
	WaveCorrectKind wave_correct = detail::WAVE_CORRECT_HORIZ;
	bool save_graph = false;
	std::string save_graph_to;
	string warp_type = "cylindrical";//"spherical"; //"cylindrical";//
	int expos_comp_type = ExposureCompensator::GAIN_BLOCKS;
	float match_conf = 0.3f;

	double work_scale = 1, seam_scale = 1, compose_scale = 1;
	bool is_work_scale_set = false; 
	bool is_seam_scale_set = false, is_compose_scale_set = false;

	LOGLN("Finding features...");
	int64 t = getTickCount();

	Ptr<FeaturesFinder> finder;
	finder = new SurfFeaturesFinder();

	vector<Mat> mask_warped1;
	vector<Mat> full_img_s(num_images); 
	vector<ImageFeatures> features(num_images);
	vector<Mat> images(num_images);
	vector<Size> full_img_sizes(num_images);
	double seam_work_aspect = 1;

	//WaitForSingleObject( hMutex , INFINITE  );
	for (int i = 0; i < num_images ; ++i)
	{
		Mat iimg;
		shareMem[i].GetMemData( iimg);
		remap( iimg, full_img_s[i], mapEye1[i], mapEye2[i] , INTER_LINEAR );
		iimg.release();

		char t[3];
		string s;
		sprintf(t, "%d", i);
		s = t;
		img_names.push_back( s );
	}
	//ReleaseMutex(hMutex);

	for (int i = 0; i < num_images ; ++i)
	{	
		full_img_sizes[i] = full_img_s[i].size();

		if (full_img_s[i].empty())
		{
			LOGLN("Can't open image " << img_names[i]);
			return 1;
		}
		if (work_megapix < 0)
		{
			images[i] = full_img_s[i];
			work_scale = 1;
			is_work_scale_set = true;
		}
		else
		{
			if (!is_work_scale_set)
			{
				work_scale = min(1.0, sqrt(work_megapix * 1e6 / full_img_s[i].size().area()));
				is_work_scale_set = true;
			}
			resize(full_img_s[i], images[i], Size(), work_scale, work_scale);
		}
		if (!is_seam_scale_set)
		{
			seam_scale = min(1.0, sqrt(seam_megapix * 1e6 / full_img_s[i].size().area()));
			seam_work_aspect = seam_scale / work_scale;
			is_seam_scale_set = true;
		}

		(*finder)(images[i], features[i]);
		features[i].img_idx = i;
		LOGLN("Features in image #" << i+1 << ": " << features[i].keypoints.size());

		resize(full_img_s[i], images[i], Size(), seam_scale, seam_scale);
	}

	finder->collectGarbage();

	LOGLN("Finding features, time: " << ((getTickCount() - t) / getTickFrequency()) << " sec");
	LOG("Pairwise matching");
	t = getTickCount();

	vector<MatchesInfo> pairwise_matches;
	BestOf2NearestMatcher matcher( false , match_conf);
	matcher(features, pairwise_matches);
	matcher.collectGarbage();
	LOGLN("Pairwise matching, time: " << ((getTickCount() - t) / getTickFrequency()) << " sec");

	// Check if we should save matches graph
#if 0
	if (save_graph)
	{
		LOGLN("Saving matches graph...");
		ofstream f(save_graph_to.c_str());
		f << matchesGraphAsString(img_names, pairwise_matches, conf_thresh);
	}
#endif 
    // Leave only images we are sure are from the same panorama
    vector<int> indices = leaveBiggestComponent(features, pairwise_matches, conf_thresh);
    vector<Mat> img_subset;
    vector<string> img_names_subset;
    vector<Size> full_img_sizes_subset;
    for (size_t i = 0; i < indices.size(); ++i)
    {
        img_names_subset.push_back(img_names[indices[i]]);
        img_subset.push_back(images[indices[i]]);
        full_img_sizes_subset.push_back(full_img_sizes[indices[i]]);
    }

    images = img_subset;
    img_names = img_names_subset;
    full_img_sizes = full_img_sizes_subset;

    // Check if we still have enough images
    if (num_images != static_cast<int>(img_names.size()))
    {
        LOGLN("These are not suitable and need more images");
        return -1;
    }

	HomographyBasedEstimator estimator;
	//vector<CameraParams> cameras;
	vector<Mat> Kcamera;
	estimator(features, pairwise_matches, cameras);
/*
	if ( imgMatready == false )
	{
		for (int  i = 0; i <  num_images ; ++i)
		{
			cameras[i].focal = camerasB[i].focal;
			cameras[i].aspect = camerasB[i].aspect;
			cameras[i].ppx = camerasB[i].ppx;
			cameras[i].ppy = camerasB[i].ppy;
			cameras[i].R = camerasB[i].R.clone();
			cameras[i].t = camerasB[i].t.clone();
		}
		//imgMatready = true;
	}
*/
	for (size_t i = 0; i < cameras.size(); ++i)
	{
		Mat R;
		cameras[i].R.convertTo(R, CV_32F);
		cameras[i].R = R;
	}
	Ptr<detail::BundleAdjusterBase> adjuster = new detail::BundleAdjusterRay();
	adjuster->setConfThresh(conf_thresh);
	Mat_<uchar> refine_mask = Mat::zeros(3, 3, CV_8U);
	if (ba_refine_mask[0] == 'x') refine_mask(0,0) = 1;
	if (ba_refine_mask[1] == 'x') refine_mask(0,1) = 1;
	if (ba_refine_mask[2] == 'x') refine_mask(0,2) = 1;
	if (ba_refine_mask[3] == 'x') refine_mask(1,1) = 1;
	if (ba_refine_mask[4] == 'x') refine_mask(1,2) = 1;

	adjuster->setRefinementMask(refine_mask);
	(*adjuster)(features, pairwise_matches, cameras);

	// Find median focal length
	vector<double> focals;
	for (size_t i = 0; i < cameras.size(); ++i)
	{
		focals.push_back(cameras[i].focal);
	}
	sort(focals.begin(), focals.end());

	float warped_image_scale;
	if (focals.size() % 2 == 1)
		warped_image_scale = static_cast<float>(focals[focals.size() / 2]);
	else
		warped_image_scale = static_cast<float>(focals[focals.size() / 2 - 1] + focals[focals.size() / 2]) * 0.5f;
	// warve correct 
	if (do_wave_correct)
	{
		vector<Mat> rmats;
		for (size_t i = 0; i < cameras.size(); ++i)
			rmats.push_back(cameras[i].R);
		waveCorrect(rmats, wave_correct);
		for (size_t i = 0; i < cameras.size(); ++i)
			cameras[i].R = rmats[i];
	}

	LOGLN("Warping images (auxiliary)... ");
	t = getTickCount();

	vector<Mat> masks_warped(num_images);
	vector<Mat> images_warped(num_images);
	vector<Size> sizes(num_images);
	vector<Mat> masks(num_images);

	// Preapre images masks
	for (int i = 0; i < num_images; ++i)
	{
		masks[i].create(images[i].size(), CV_8U);
		masks[i].setTo(Scalar::all(255));
	}

	// Warp images and their masks
	Ptr<WarperCreator> warper_creator;
	if(warp_type == "cylindrical") warper_creator = new cv::CylindricalWarper();
	else if (warp_type == "spherical") warper_creator = new cv::SphericalWarper();
    
	if (warper_creator.empty())
	{
		cout << "Can't create the following warper '" << warp_type << "'\n";
		return 1;
	}

	Ptr<RotationWarper> warper = warper_creator->create(static_cast<float>(warped_image_scale * seam_work_aspect));

	for (int i = 0; i < num_images; ++i)
	{
		Mat_<float> K;
		cameras[i].K().convertTo(K, CV_32F);
		float swa = (float)seam_work_aspect;
		K(0,0) *= swa; K(0,2) *= swa;
		K(1,1) *= swa; K(1,2) *= swa;
		corners[i] = warper->warp(images[i], K, cameras[i].R, INTER_LINEAR, BORDER_REFLECT, images_warped[i]);
		sizes[i] = images_warped[i].size();
		warper->warp(masks[i], K, cameras[i].R, INTER_NEAREST, BORDER_CONSTANT, masks_warped[i]);
	}

	vector<Mat> images_warped_f(num_images);
	for (int i = 0; i < num_images; ++i)
		images_warped[i].convertTo(images_warped_f[i], CV_32F);

	LOGLN("Warping images, time: " << ((getTickCount() - t) / getTickFrequency()) << " sec");

	if (compensator == NULL ) 
		compensator = ExposureCompensator::createDefault(expos_comp_type);
	compensator->feed(corners, images_warped, masks_warped);

	Ptr<SeamFinder> seam_finder;
	seam_finder = new detail::GraphCutSeamFinder(GraphCutSeamFinderBase::COST_COLOR);
	if (seam_finder.empty())
	{
		cout << "Can't create the following seam finder " << "'\n";
		return 1;
	}

	seam_finder->find(images_warped_f, corners, masks_warped);

	// Release unused memory
	images.clear();
	images_warped.clear();
	images_warped_f.clear();
	masks.clear();

	LOGLN("Compositing...");
	t = getTickCount();

	Mat img;
	Mat img_warped, img_warped_s;
	Mat dilated_mask, seam_mask, mask, mask_warped;
	double compose_work_aspect = 1;

	for (int img_idx = 0; img_idx < num_images; ++img_idx)
	{
		if (!is_compose_scale_set)
		{
			if (compose_megapix > 0)
				compose_scale = min(1.0, sqrt(compose_megapix * 1e6 / full_img_s[img_idx].size().area()));
			is_compose_scale_set = true;
			// Compute relative scales
			//compose_seam_aspect = compose_scale / seam_scale;
			compose_work_aspect = compose_scale / work_scale;

			// Update warped image scale
			warped_image_scale *= static_cast<float>(compose_work_aspect);
			warper = warper_creator->create(warped_image_scale);

			// Update corners and sizes
			for (int i = 0; i < num_images; ++i)
			{
				// Update intrinsics
				cameras[i].focal *= compose_work_aspect;
				cameras[i].ppx *= compose_work_aspect;
				cameras[i].ppy *= compose_work_aspect;
				// Update corner and size
				Size sz = full_img_sizes[i];
				if (std::abs(compose_scale - 1) > 1e-1)
				{
					sz.width = cvRound(full_img_sizes[i].width * compose_scale);
					sz.height = cvRound(full_img_sizes[i].height * compose_scale);
				}

				Mat K;
				cameras[i].K().convertTo(K, CV_32F);
				Rect roi = warper->warpRoi(sz, K, cameras[i].R);
				corners[i] = roi.tl();
				sizes[i] = roi.size();
			}
		}
		if (abs(compose_scale - 1) > 1e-1)
			resize(full_img_s[img_idx], img, Size(), compose_scale, compose_scale);
		else
			img = full_img_s[img_idx];
		srctype = img.type();
		full_img_s[img_idx].release();
		Size img_size = img.size();

		Mat Kt;
		cameras[img_idx].K().convertTo( Kt, CV_32F);
		Kcamera.push_back(Kt);
		// Warp the current image mask
		mask.create(img_size, CV_8U);
		mask.setTo(Scalar::all(255));
 
		warpsize( warper , mask, Kcamera.at(img_idx), cameras[img_idx].R, INTER_NEAREST, BORDER_CONSTANT, mask_warped  , xmap1[img_idx], ymap1[img_idx]);
		// Warp the current image	
		dst_roi[img_idx] = warpsize( warper , img,  Kcamera.at(img_idx), cameras[img_idx].R, INTER_LINEAR, BORDER_REFLECT, img_warped , xmap1[img_idx], ymap1[img_idx] );
		if (dst_roi[img_idx].width == 0)
		{
			LOGLN("Finished, Warpsize is error width 0 " );
			return -1;
		}	
		img.release();
		// Compensate exposure
		compensator->apply(img_idx, corners[img_idx], img_warped, mask_warped);
		img_warped.convertTo(img_warped_s, CV_16S);
		img_warped.release();
		mask.release();

		//img_warped_s  mask_warped
		dilate(masks_warped[img_idx], dilated_mask, Mat());
		resize(dilated_mask, seam_mask, mask_warped.size());
		mask_warped = seam_mask & mask_warped;
				
		mask_warped1.push_back(mask_warped);
		if( img_idx == 0 )
			prepareSize( resultRoiSize(corners, sizes) ,   result1, result_mask1 , dst_roi_1  );
		// Blend the current image
		feedSize(img_warped_s, mask_warped, corners[img_idx] , dst_roi_1 , result1, result_mask1 ,img_idx );
	}

	Mat result2, result_mask2;
	blendSize(result2, result_mask2 ,  result1, result_mask1   );
	//LOGLN("Compositing, time: " << ((getTickCount() - t) / getTickFrequency()) << " sec");
	//LOGLN("Finished, total time: " << ((getTickCount() - app_start_time) / getTickFrequency()) << " sec");		
	//imwrite("prestitch.jpg", result1);
	Upflag = true;
	return 0;
}

int UpdateMat()
{
	if( ResultStitch.data != NULL  )
	{
		ResultStitch.release();
	}
	ResultStitch.create(dst_roi_1.size(), CV_16SC3);
	ResultStitch.setTo(Scalar::all(0));

	Result_mask_stitch = result_mask1.clone();
	xmapdetstitch = xmapdet.clone();
	ymapdetstitch = ymapdet.clone();
	imgMatready = true;
	indxmapstitch = indxmap.clone();

	for(int i = 0 ; i < num_images ; i++ )
	{
		xmapstitch[i] = xmap1[i].clone();
		ymapstitch[i] = ymap1[i].clone();
		if( img4[i].data != NULL)
		{
			img4[i].release();
		}
		img4[i].create(dst_roi[i].height + 1, dst_roi[i].width + 1, srctype );
	}
	return 0;
}

DWORD WINAPI StitchingAll(   )
{
	vector<Mat> img3(num_images);
	vector<Mat> img5(num_images);
	int frmnum = 1;
	unsigned char c;
	namedWindow("MyPicture",0);
	moveWindow("Scribble Image", 300,300);  
	resizeWindow("MyPicture" , 1300 , 400  );
	//WaitForSingleObject( hMutex , INFINITE  );
	for (int i = 0; i < num_images ; ++i)
	{
		Mat iimg;
		shareMem[i].GetMemData( iimg);
		remap( iimg , img3[i] , mapEye1[i], mapEye2[i] , INTER_LINEAR );

		if (img3[i].data == NULL )
				return -1;
	}
	//ReleaseMutex(hMutex);		
	for (int img_idx = 0; img_idx < num_images ; ++img_idx)
	{
		Size img_size = img3[img_idx].size();
		remap(img3[img_idx], img4[img_idx],  xmapstitch[img_idx], ymapstitch[img_idx],  INTER_LINEAR, BORDER_REFLECT );		
		compensator->apply(img_idx, corners[img_idx] , img4[img_idx], img3[img_idx]);
		img4[img_idx].convertTo( img5[img_idx]  , CV_16S  );		
	}
	feedSizeRemap( img5 ,  ResultStitch , Result_mask_stitch  );

	Mat temp; 
	ResultStitch.convertTo( temp  , CV_8U  );
	imshow("MyPicture", temp );
	c =	waitKey(30);
    if (c == 27)        // 按下Esc退出播放  
    {  return -1;  
    }else if( c == 'a')
	{  
		imgMatready = false;
	}
	return 0;
}

DWORD WINAPI GetPreStructw( LPVOID lpParameter )
{	
	int ret = 0;
	while(1)
	{
		WaitForSingleObject( hEventPreStitch , INFINITE );			
    	ret = GetPreStruct( lpParameter );
		ResetEvent( hEventPreStitch );
	}
	result1.release();
	result_mask1.release();
	return 0;
}
void OpenFFmgeg()
{
	WinExec( "D:\\14-3D摄像机方案\\opencv-ip-sample\\GetFromRtsp\\x64\\Debug\\GetFromRtsp.exe rtsp://169.254.49.14:554/user=admin&password=&channel=1&stream=0.sdp?real_stream ShareMemoryRTSPData0 SM_MutexRTSPData0 ServerWriteOver0 ClientReadOver0  " , SW_HIDE );
	WinExec( "D:\\14-3D摄像机方案\\opencv-ip-sample\\GetFromRtsp\\x64\\Debug\\GetFromRtsp.exe rtsp://169.254.49.12:554/user=admin&password=&channel=1&stream=0.sdp?real_stream ShareMemoryRTSPData1 SM_MutexRTSPData1 ServerWriteOver1 ClientReadOver1  " , SW_HIDE );
	WinExec( "D:\\14-3D摄像机方案\\opencv-ip-sample\\GetFromRtsp\\x64\\Debug\\GetFromRtsp.exe rtsp://169.254.49.11:554/user=admin&password=&channel=1&stream=0.sdp?real_stream ShareMemoryRTSPData2 SM_MutexRTSPData2 ServerWriteOver2 ClientReadOver2  " , SW_HIDE );
	Sleep(10000);
}

int OpenVideo()
{
	Mat temp;
	for( int i = 0 ; i < num_images ; i++ )
	{
		shareMem[i].GetMemData( temp );
		RunTestEyeFishMap( temp.size()  , i);  //筒形畸形矫正
	}
	return 0;
}


int main(int argc, char* argv[])
{
	float PreTime = 0;
	float t = 0;
	OpenFFmgeg();

	shareMem[0].ShareMemPre();
	shareMem[1].ShareMemPre();
	shareMem[2].ShareMemPre();
	hMutex = CreateMutex( NULL , TRUE , NULL ); //读取文件保护
	if ( OpenVideo() != 0 )
	{	
		return 0;
	}
	while(1)
	{
		if ( GetPreStruct( NULL ) == 0 )
			break;
	}
	UpdateMat();
	ReleaseMutex(hMutex);
	hEventPreStitch = CreateEvent(NULL,FALSE,TRUE , L"SetEnvernt");
	HANDLE handlePreStruct = CreateThread( NULL , 0 ,  GetPreStructw , NULL , 0 , NULL ); 

	while( 1 )
	{
		t = getTickCount();
		if (Upflag == true)
		{UpdateMat();Upflag = false;}

		int ret = StitchingAll();
		if (ret == -1)
			break;

		PreTime += ((getTickCount() - t) / getTickFrequency()) ;		
		if ( PreTime > 30 )
		{
			SetEvent( hEventPreStitch );
			PreTime = 0;
		}
	}

	CloseHandle(handlePreStruct);
	shareMem[0].SharememClose();
	shareMem[1].SharememClose();
	shareMem[2].SharememClose();
	return 0;
}





