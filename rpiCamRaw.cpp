#include <stdio.h>
#include <string>
#include "misc/image.hpp"
#include "misc/exifRead.hpp"
#include "log/log.hpp"

int main(int argc, char** argv)
{
   double* hdrmean=0;
   double* hdrvar=0;
   SImg img;
   for (int i = 1; i < argc; ++i)
   {
      TRACE(argv[i]);

      FILE* ifp = fopen(argv[i], "rb");
      if (!ifp)
      {
         return false;
      }
      char head[33];
      if (!fseek(ifp, -10270208, SEEK_END))
      {
          if (fread(head, 1, 32, ifp) && !strcmp(head, "BRCMo"))
          {
               long int data_offset = ftell(ifp) + 0x8000 - 32;
               img = readRPI(argv[i], data_offset, 0);
          }
          else
          {
             WARNINGH("SKIPPING FILE BRCMo not found");
          }
      }
      else
      {
         WARNINGH("SKIPPING FILE SEEK_END-10270208 failed");
         WARNINGH(fseek(ifp, 0, SEEK_END));
         WARNINGH(ftell(ifp));
         continue;
      }

      if (0 == hdrmean)
      {
         hdrmean = new double[img.mWidth*img.mHeight*3]();
         hdrvar  = new double[img.mWidth*img.mHeight*3]();
      }

      SExifData data(getExifData(argv[i]));

      unsigned short* pix = reinterpret_cast<unsigned short*>(img.mData);

      double scale = 1. / (data.mExposureTime.getDouble() + 0.000016);

      for (int i = 0; i<img.mWidth*img.mHeight*img.mChannels; ++i)
      {
         double value = pix[i];
         if (1 > value) continue;
         if (958 < value) continue;

         double G = 10748.9;
         double r = 5.72988;
         double p = 0.0344954;

         double x = value / 958;
         double snr = (G*x) / sqrt(r*r + G*x + (p*G*x)*(p*G*x));

         double var = value/snr;
         value *= scale;
         var *= scale;
         if (0 == hdrmean[i] && 0 == hdrvar[i])
         {
            hdrmean[i] = value;
            hdrvar[i] = var;
         }
         else
         {
            hdrmean[i] = (value*hdrvar[i]+hdrmean[i]*var)/(hdrvar[i]+var);
            hdrvar[i] = var*hdrvar[i]/(var+hdrvar[i]);
         }
      }
      delete[] pix;
   }

   if (!hdrmean)
   {
      ERRORH("NO RAW RPI INPUT: NO OUTPUT GENERATED");
   }
   img.setRGBE(hdrmean);

   writePNG("hdr.png", img);

   double* snrmean = new double[3096]();
   int* snrn = new int[3096]();

   // evaluate the SNR
   for (int i = 1; i < argc; ++i)
   {
      TRACE(argv[i]);

      FILE* ifp = fopen(argv[i], "rb");
      if (!ifp)
      {
         return false;
      }
      char head[33];
      if (!fseek(ifp, -10270208, SEEK_END))
      {
         if (fread(head, 1, 32, ifp) && !strcmp(head, "BRCMo"))
         {
            long int data_offset = ftell(ifp) + 0x8000 - 32;
            img = readRPI(argv[i], data_offset, 0);
         }
         else
         {
            WARNINGH("SKIPPING FILE CRCMo not found");
         }
      }
      else
      {
         WARNINGH("SKIPPING FILE SEEK_END-10270208 failed");
         WARNINGH(fseek(ifp, 0, SEEK_END));
         WARNINGH(ftell(ifp));
         continue;
      }

      SExifData data(getExifData(argv[i]));

      unsigned short* pix = reinterpret_cast<unsigned short*>(img.mData);

      double scale = 1. / (data.mExposureTime.getDouble() + 0.000016);

      for (int i = 0; i<img.mWidth*img.mHeight*img.mChannels; ++i)
      {
         double value = pix[i];
         if (0 == hdrmean[i] && 0 == hdrvar[i])
         {
            WARNINGH("UNKNOWN Reference");
         }
         else
         {
            double signal = hdrmean[i] / scale;
            double noise = abs(signal - value);

            int bin = static_cast<int>(signal*10. + .5);
            if (bin > 1137) bin = 1024 + static_cast<int>(signal + .5);
            if (bin > 3095) bin = 3095;

            ++snrn[bin];
            snrmean[bin] += (noise*noise - snrmean[bin]) / snrn[bin];
         }
      }
      delete[] pix;
   }

   for (int i = 0; i < 3096; ++i)
   {
      printf("%g,%g,%i\n", i < 1138 ? i / 10.0 : i - 1024, snrmean[i], snrn[i]);
   }

   return 0;
}
