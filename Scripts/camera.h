#ifndef CAMERA_H
#define CAMERA_H

#include "hittable.h"
#include "material.h"
#include <vector>
#include <thread>



class camera{
    public:
    double aspect_ratio = 1.0;
    int image_width = 100;
    int samples_per_pixel = 10;
    int max_depth = 10; //Maximum number of ray bounces into scene
    


    double vfov = 90; //Vertical fov
    point3 lookfrom = point3(0,0,0); //point camera looks from
    point3 lookat = point3(0,0,-1); //point camera is looking at
    vec3 vup = vec3(0,1,0); //Camera-relative "up" direction

    double defocus_angle = 0;  // Variation angle of rays through each pixel
    double focus_dist = 10;    // Distance from camera lookfrom point to plane of perfect focus

    void render(const hittable& world){
        initialize();

        
        int number_of_threads = std::thread::hardware_concurrency();
        std::vector<std::thread> threads;
        std::vector<std::vector<color>> image(image_height, std::vector<color>(image_width));
        int rows_per_thread = image_height/number_of_threads;
        std::clog << number_of_threads << " Threads Initialised";
        for (int t = 0; t < number_of_threads; ++t) {
            int start = t * rows_per_thread;
            int end = (t == number_of_threads - 1) ? image_height : start + rows_per_thread;
            threads.emplace_back([this,t, &image, start, end, &world]() {
                render_rows(t,image, start, end, world);
            });
        }


        std::cout << "P3\n" << image_width << " " << image_height << "\n255\n";
        // Wait for all threads to finish
        for (auto& t : threads) {
            t.join();
        }

        for(int j=0;j<image_height;j++){
            for(int i=0;i<image_width;i++){
                write_color(std::cout,pixel_samples_scale*image[j][i]);
            }
        }
        std::clog << "\rDone.                 \n";
    }

    void render_rows(int thread_num, std::vector<std::vector<color>> &image, int start_height, int end_height, const hittable& world){
        for (int j = start_height; j < end_height ;++j){
            for(int i =0; i<image_width; ++i){
                color pixel_color(0,0,0);
                for (int sample=0; sample<samples_per_pixel; sample++){
                    ray r = get_ray(i,j);
                    pixel_color += ray_color(r,max_depth,world);
                }
                image[j][i] = pixel_color;
                

            }
            std::clog << "\rThread " << thread_num << ": " << (end_height - 1 - j) << " rows remaining." << std::flush;
        }
    }

    private:
    int image_height; //Rendered image height
    double pixel_samples_scale; //colour scale factor for a sum of pixel samples
    point3 center; //Camera center
    point3 pixel00_loc; //Location of pixel 0,0
    vec3 pixel_delta_u; //Offset of pixel to the right
    vec3 pixel_delta_v; //Offset to pixel below
    vec3 u,v,w; //Camera frame basis vectors
    vec3   defocus_disk_u;       // Defocus disk horizontal radius
    vec3   defocus_disk_v;       // Defocus disk vertical radius

    void initialize(){
        image_height = int(image_width/aspect_ratio);
        image_height = (image_height < 1) ? 1 : image_height;

        pixel_samples_scale = 1.0 / samples_per_pixel;

        center = lookfrom;

        // Determine viewport dimensions.
        auto theta = degrees_to_radians(vfov);
        auto h = std::tan(theta/2);
        auto viewport_height = 2*h * focus_dist;
        auto viewport_width = viewport_height * (double(image_width)/image_height);

        //Calculate u v w unit basis vectors for camera coordinate frame
        w = unit_vector(lookfrom-lookat);
        u = unit_vector(cross(vup,w));
        v = cross(w,u);

        // Calculate the vectors across the horizontal and down the vertical viewport edges.
        auto viewport_u = viewport_width*u;
        auto viewport_v = viewport_height*-v;

        // Calculate the horizontal and vertical delta vectors from pixel to pixel.
        pixel_delta_u = viewport_u / image_width;
        pixel_delta_v = viewport_v / image_height;

        // Calculate the location of the upper left pixel.
        auto viewport_upper_left =
            center - (focus_dist*w)- viewport_u/2 - viewport_v/2;
        pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);
    
        //Calculate camera defocus disk basis vectors
        auto defocus_radius = focus_dist * std::tan(degrees_to_radians(defocus_angle / 2));
        defocus_disk_u = u * defocus_radius;
        defocus_disk_v = v * defocus_radius;

    }

    ray get_ray(int i, int j) const{
         // Construct a camera ray originating from the defocus disk and directed at a randomly
        // sampled point around the pixel location i, j.

        auto offset = sample_square();
        auto pixel_sample = pixel00_loc
                          + ((i + offset.x()) * pixel_delta_u)
                          + ((j + offset.y()) * pixel_delta_v);

        auto ray_origin = (defocus_angle <= 0) ? center : defocus_disk_sample();
        auto ray_direction = pixel_sample - ray_origin; 

        return ray(ray_origin, ray_direction);
    }
    

    vec3 sample_square() const{
        //Returns vector to a random point in the unit square
        return vec3(random_double()-0.5,random_double()-0.5,0);
    }

    point3 defocus_disk_sample() const {
        // Returns a random point in the camera defocus disk.
        auto p = random_in_unit_disk();
        return center + (p[0] * defocus_disk_u) + (p[1] * defocus_disk_v);
    }

    color ray_color(const ray& r,int depth, const hittable& world) {
        //If ray bounces more than max depth, no more light is gathered, return black
        if (depth <= 0) {
            return color(0, 0, 0);
        }

        hit_record rec;
        if (world.hit(r, interval(0.001, infinity), rec)) { // 0.001 to prevent reinteraction with surface
            ray scattered;
            color attenuation;
            if (rec.mat->scatter(r, rec, attenuation, scattered)){
                return attenuation * ray_color(scattered, depth-1, world);
            }
            return color(0,0,0);
        }

        //The sky
        vec3 unit_direction = unit_vector(r.direction()); 
        auto a = 0.5*(unit_direction.y() + 1.0);
        return (1.0-a)*color(1.0, 1.0, 1.0) + a*color(0.5, 0.7, 1.0);

    }
};

#endif